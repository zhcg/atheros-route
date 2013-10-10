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
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"                    /* NB: for HAL_PHYERR* */
#endif
#include "ar9300/ar9300.h"
#include "ar9300/ar9300templateGeneric.h"
#include "ar9300/ar9300templateXB112.h"
#include "ar9300/ar9300templateHB116.h"
#include "ar9300/ar9300templateXB113.h"
#include "ar9300/ar9300templateHB112.h"
#include "ar9300/ar9300templateAP121.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300phy.h"

#ifdef MDK_AP
#include "instance.h"
#endif

#ifdef UNUSED
#ifdef ART_BUILD
extern int calData;
#endif
#endif

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
void ar9300SwapEeprom(ar9300_eeprom_t *eep);
void ar9300EepromTemplateSwap(void);
#endif
#define N(a) (sizeof (a) / sizeof (a[0]))

static u_int16_t ar9300EepromGetSpurChan(struct ath_hal *ah, u_int16_t spurChan,HAL_BOOL is2GHz);
#ifdef UNUSED
static inline HAL_BOOL ar9300FillEeprom(struct ath_hal *ah);
static inline HAL_STATUS ar9300CheckEeprom(struct ath_hal *ah);
#endif


static ar9300_eeprom_t *default9300[]=
{
    &Ar9300TemplateGeneric,
    &Ar9300TemplateXB112,
    &Ar9300TemplateHB116,
    &Ar9300TemplateHB112,
    &Ar9300TemplateXB113,
    &Ar9300TemplateAP121,
};

/*
 * This is where we look for the calibration data. must be set before ath_attach() is called
 */
static int CalibrationDataTry = CalibrationDataNone;
static int CalibrationDataTryAddress = 0;

/*
 * Set the type of memory used to store calibration data.
 * Used by nart to force reading/writing of a specific type.
 * The driver can normally allow autodetection by setting source to CalibrationDataNone=0.
 */
void ar9300CalibrationDataSet(struct ath_hal *ah, int32_t source)
{
	struct ath_hal_9300 *ahp;
	if(ah!=0)
	{
		ahp = AH9300(ah);
		ahp->CalibrationDataSource=source;
	}
	else
	{
		CalibrationDataTry=source;
	}
}

int32_t ar9300CalibrationDataGet(struct ath_hal *ah)
{
	struct ath_hal_9300 *ahp;
	if(ah!=0)
	{
		ahp = AH9300(ah);
		return ahp->CalibrationDataSource;
	}
	else
	{
		return CalibrationDataTry;
	}
}

/*
 * Set the address of first byte used to store calibration data.
 * Used by nart to force reading/writing at a specific address.
 * The driver can normally allow autodetection by setting size=0.
 */
void ar9300CalibrationDataAddressSet(struct ath_hal *ah, int32_t size)
{
	struct ath_hal_9300 *ahp;
	if(ah!=0)
	{
		ahp = AH9300(ah);
		ahp->CalibrationDataSourceAddress=size;
	}
	else
	{
		CalibrationDataTryAddress=size;
	}
}

int32_t ar9300CalibrationDataAddressGet(struct ath_hal *ah)
{
	struct ath_hal_9300 *ahp;
	if(ah!=0)
	{
		ahp = AH9300(ah);
		return ahp->CalibrationDataSourceAddress;
	}
	else
	{
		return CalibrationDataTryAddress;
	}
}

/*
 * This is the template that is loaded if Ar9300EepromRestore() can't find valid data in the memory.
 */
static int Ar9300EepromTemplatePreference=Ar9300EepromTemplateGeneric;

void ar9300EepromTemplatePreference(int32_t value)
{
	Ar9300EepromTemplatePreference=value;
}

/*
 * Install the specified default template.
 * Overwrites any existing calibration and configuration information in memory.
 */
int32_t ar9300EepromTemplateInstall(struct ath_hal *ah, int32_t value)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    ar9300_eeprom_t *mptr, *dptr;
    int mdataSize;

    mptr = &ahp->ah_eeprom;
    mdataSize = ar9300EepromStructSize();
	if(mptr!=0)
	{
//		ahp->CalibrationDataSource=CalibrationDataNone;
// 		ahp->CalibrationDataSourceAddress=0;
		dptr=ar9300EepromStructDefaultFindById(value);
        if (dptr!=0)
        {
		    OS_MEMCPY(mptr,dptr,mdataSize);
			return 0;
        }
	}
	return -1;
}

static int
ar9300EepromRestoreSomething(struct ath_hal *ah, ar9300_eeprom_t *mptr, int mdataSize)
{
	int it;
	ar9300_eeprom_t *dptr;
	int nptr;

	nptr = -1; 
    //
    // if we didn't find any blocks in the memory, put the prefered template in place
    //
    if (nptr<0)
    {
		AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
		AH9300(ah)->CalibrationDataSourceAddress=0;
        dptr=ar9300EepromStructDefaultFindById(Ar9300EepromTemplatePreference);
        if (dptr!=0)
        {
		    OS_MEMCPY(mptr,dptr,mdataSize);	
            nptr=0;
        }
    }
    //
    // if we didn't find the prefered one, put the normal default template in place
    //
    if (nptr<0)
    {
		AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
		AH9300(ah)->CalibrationDataSourceAddress=0;
        dptr=ar9300EepromStructDefaultFindById(Ar9300EepromTemplateDefault);
        if (dptr!=0)
        {
		    OS_MEMCPY(mptr,dptr,mdataSize);	
            nptr=0;
        }
    }
    //
    // if we can't find the best template, put any old template in place
    // presume that newer ones are better, so search backwards
    //
    if (nptr<0)
    {
		AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
		AH9300(ah)->CalibrationDataSourceAddress=0;
	    for (it=ar9300EepromStructDefaultMany()-1; it>=0; it--)
	    {
            dptr=ar9300EepromStructDefault(it);
            if (dptr!=0)
            {
			    OS_MEMCPY(mptr,dptr,mdataSize);	
				nptr=0;
			    break;
		    }
	    }
    }

	return nptr;
}


/*
 * Read 16 bits of data from offset into *data
 */
HAL_BOOL
ar9300EepromReadWord(struct ath_hal *ah, u_int off, u_int16_t *data)
{
	if (AR_SREV_OSPREY(ah) || AR_SREV_POSEIDON(ah))
	{
		(void)OS_REG_READ(ah, AR9300_EEPROM_OFFSET + (off << AR9300_EEPROM_S));
		if (!ath_hal_wait(ah, AR_HOSTIF_REG(ah, AR_EEPROM_STATUS_DATA), AR_EEPROM_STATUS_DATA_BUSY
			| AR_EEPROM_STATUS_DATA_PROT_ACCESS, 0, AH_WAIT_TIMEOUT))
		{
			return AH_FALSE;
		}
		*data = MS(OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_EEPROM_STATUS_DATA)), AR_EEPROM_STATUS_DATA_VAL);
		return AH_TRUE;
	} 
	else 
	{
		*data=0;
		return AH_FALSE;
	}
}

#ifdef AH_SUPPORT_WRITE_EEPROM
   
/*
 * Write 16 bits of data from data to the specified EEPROM offset.
 */
HAL_BOOL
ar9300EepromWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    u_int32_t status;
    u_int32_t write_to = 50000;      /* write timeout */
    u_int32_t eepromValue;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t eepromWriteGPIO = ahp->ah_eeprom.baseEepHeader.eepromWriteEnableGpio;
    u_int32_t outputMuxRegister = AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX1);
    u_int32_t muxBitPosition = 15;  //old default of GPIO-3
    u_int32_t configRegister = AR_HOSTIF_REG(ah, AR_GPIO_OE_OUT);
    u_int32_t configBitPosition = 2 * eepromWriteGPIO;

	if (AR_SREV_OSPREY(ah) || AR_SREV_POSEIDON(ah)) 
	{
		eepromValue = (u_int32_t)data & 0xffff;
		/* get the correct registers and bit position for GPIO configuration */
		if (eepromWriteGPIO <= 5)
		{
			outputMuxRegister = AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX1);
			muxBitPosition = eepromWriteGPIO * 5;
		} 
		else if ((eepromWriteGPIO >= 6) && (eepromWriteGPIO <= 11))
		{
			outputMuxRegister = AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX2);
			muxBitPosition = (eepromWriteGPIO-6) * 5;
		}
		else if ((eepromWriteGPIO >= 12) && (eepromWriteGPIO <= 16))
		{
			outputMuxRegister = AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX3);
			muxBitPosition = (eepromWriteGPIO-12) * 5;
		}

		if (eepromWriteGPIO == 16) 
		{
			configRegister = AR_HOSTIF_REG(ah, AR_GPIO_OE1_OUT);
			configBitPosition = 0;
		}

		/* Setup EEPROM device to write */
		OS_REG_RMW(ah, outputMuxRegister, 0, 0x1f << muxBitPosition);   /* Mux GPIO-3 as GPIO */
		OS_DELAY(1);
		OS_REG_RMW(ah, configRegister, 0x3 << configBitPosition, 0x3 << configBitPosition);     /* Configure GPIO-3 as output */
		OS_DELAY(1);
#ifdef AR9300_EMULATION
		HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "ar9300EepromWrite: FIXME\n");
#else
		OS_REG_RMW(ah, AR_GPIO_IN_OUT, 0, 1 << eepromWriteGPIO);       /* drive GPIO-3 low */
#endif
		OS_DELAY(1);

		/* Send write data, as 32 bit data */
		OS_REG_WRITE(ah, AR9300_EEPROM_OFFSET + (off << AR9300_EEPROM_S), eepromValue);

		/* check busy bit to see if eeprom write succeeded */
		while (write_to > 0) {
			status = OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_EEPROM_STATUS_DATA)) &
										(AR_EEPROM_STATUS_DATA_BUSY |
										 AR_EEPROM_STATUS_DATA_BUSY_ACCESS |
										 AR_EEPROM_STATUS_DATA_PROT_ACCESS |
										 AR_EEPROM_STATUS_DATA_ABSENT_ACCESS);
			if (status == 0) {
#ifndef AR9300_EMULATION
				OS_REG_RMW(ah, AR_GPIO_IN_OUT, 1<<eepromWriteGPIO, 1<<eepromWriteGPIO);       /* drive GPIO-3 hi */
#endif
				return AH_TRUE;
			}
			OS_DELAY(1);
			write_to--;
		}

#ifndef AR9300_EMULATION
		OS_REG_RMW(ah, AR_GPIO_IN_OUT, 1<<eepromWriteGPIO, 1<<eepromWriteGPIO);       /* drive GPIO-3 hi */
#endif
		return AH_FALSE;
	}
	else
	{
		return AH_FALSE;
	}
}
#endif


HAL_BOOL
ar9300OtpRead(struct ath_hal *ah, u_int off, u_int32_t *data)
{
	int timeOut=1000;
	int status=0;

	if (AR_SREV_OSPREY(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_HORNET(ah)) 
	{
		(void)OS_REG_READ(ah, OTP_MEM_START_ADDRESS + (off*4)); // OTP is 32 bit addressable

		while ((timeOut > 0) && (!status)) {                               //# wait for access complete
		// Read data valid, access not busy, sm not busy
			status = (((OS_REG_READ(ah,OTP_STATUS0_OTP_SM_BUSY))& 0x7) == 0x4) ? 1 : 0;  // 
			timeOut--;
		}
		if (timeOut == 0) {
			HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Timed out during OTP Status0 validation\n", __func__);
			return AH_FALSE;
		}

		*data = OS_REG_READ(ah, OTP_STATUS1_EFUSE_READ_DATA);
		return AH_TRUE;
	}
	else if(AR_SREV_WASP(ah))
	{
		(void)OS_REG_READ(ah, OTP_MEM_START_ADDRESS_WASP + (off*4)); // OTP is 32 bit addressable
		
		while ((timeOut > 0) && (!status)) {                               //# wait for access complete
		// Read data valid, access not busy, sm not busy
			  status = (((OS_REG_READ(ah,OTP_STATUS0_OTP_SM_BUSY_WASP))& 0x7) == 0x4) ? 1 : 0;
			timeOut--;
		}
		if (timeOut == 0) {
			HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Timed out during OTP Status0 validation\n", __func__);
			return AH_FALSE;
		}
		*data = OS_REG_READ(ah, OTP_STATUS1_EFUSE_READ_DATA_WASP);
		return AH_TRUE;

	}
	else	
	{
		*data=0;
		return AH_FALSE;
	}
}


#ifdef AH_SUPPORT_WRITE_EEPROM

HAL_BOOL
ar9300OtpWrite(struct ath_hal *ah, u_int off, u_int32_t data)
{
	if (AR_SREV_OSPREY(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_HORNET(ah)) 
	{
		// Power on LDO
		OS_REG_WRITE(ah,OTP_LDO_CONTROL_ENABLE, 0x1);
		OS_DELAY(1000);			// was 10000
		if (!(OS_REG_READ(ah,OTP_LDO_STATUS_POWER_ON)&0x1)){
			return AH_FALSE;
		}
		OS_REG_WRITE(ah,OTP_INTF0_EFUSE_WR_ENABLE_REG_V, 0x10AD079);
		OS_DELAY(1000);			// was 1000000
		OS_REG_WRITE(ah,OTP_MEM_START_ADDRESS+(off*4),data); // OTP is 32 bit addressable

		while((OS_REG_READ(ah,OTP_STATUS0_OTP_SM_BUSY))&0x1) { 
			  OS_DELAY(1000) ;	// was 100000
			}
		OS_DELAY(1000);			// was 100000

		// Power Off LDO
		OS_REG_WRITE(ah,OTP_LDO_CONTROL_ENABLE, 0x0);
		OS_DELAY(1000);		// was 10000
		if ((OS_REG_READ(ah,OTP_LDO_STATUS_POWER_ON)&0x1)){
			return AH_FALSE;
		}
		OS_DELAY(1000);			// was 10000
		return AH_TRUE;
	}
	else if(AR_SREV_WASP(ah))
	{
		OS_REG_WRITE(ah,OTP_LDO_CONTROL_ENABLE_WASP, 0x1);
		OS_DELAY(1000);			// was 10000
		if (!(OS_REG_READ(ah,OTP_LDO_STATUS_POWER_ON_WASP)&0x1)){
			return AH_FALSE;
		}
		OS_REG_WRITE(ah,OTP_INTF0_EFUSE_WR_ENABLE_REG_V_WASP, 0x10AD079); // set 0x1 also works
		OS_DELAY(1000);			// was 1000000
	  	OS_REG_WRITE(ah,OTP_MEM_START_ADDRESS_WASP+(off*4),data); // OTP is 32 bit addressable
	  	while(((OS_REG_READ(ah,OTP_STATUS0_OTP_SM_BUSY_WASP))&0x3) != 0) { 
			OS_DELAY(1000) ;	// was 100000
		}
		OS_DELAY(1000);			// was 100000
	
		// Power Off LDO
		OS_REG_WRITE(ah,OTP_LDO_CONTROL_ENABLE_WASP, 0x0);
		OS_DELAY(1000);		// was 10000
		if ((OS_REG_READ(ah,OTP_LDO_STATUS_POWER_ON_WASP)&0x1)){
			return AH_FALSE;
		}
		OS_DELAY(1000);			// was 10000
		return AH_TRUE;
		
	}
	else
	{
		return AH_FALSE;
	}

}

#endif /* AH_SUPPORT_WRITE_EEPROM */


#ifndef WIN32

static HAL_STATUS
ar9300FlashMap(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
#ifdef AR9100
    ahp->ah_cal_mem = OS_REMAP(ah, AR9300_EEPROM_START_ADDR, AR9300_EEPROM_MAX);
#else
    ahp->ah_cal_mem = OS_REMAP((uintptr_t)(ah->ah_st), (AR9300_EEPROM_MAX+AR9300_FLASH_CAL_START_OFFSET));
#endif
    if (!ahp->ah_cal_mem)
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot remap eeprom region \n", __func__);
        return HAL_EIO;
    }

    return HAL_OK;
}

HAL_BOOL
ar9300FlashRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    *data = ((u_int16_t *)ahp->ah_cal_mem)[off];
    return AH_TRUE;
}

HAL_BOOL
ar9300FlashWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    ((u_int16_t *)ahp->ah_cal_mem)[off] = data;
    return AH_TRUE;
}
#endif /* WIN32 */

#ifdef UNUSED
#ifdef ART_BUILD   
u_int16_t ar9300CaldataTypeDetect(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
	u_int8_t value[2];
    u_int16_t *svalue;
    
    // Determine storage type.
    // If storage type is automatic, search for magic number??
    // If magic number is found record associated storage type.
    // If un-initialized number (0xffff or 0x0000) record associated storage type.
    // NEED TO DO MORE WORK ON THIS
    if (calData == CALDATA_AUTO)
    {
        svalue = (u_int16_t *)value;
        if (!ahp->ah_priv.priv.ah_eepromRead(ah, 0, svalue)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read eeprom region \n", __func__);
			return AH_FALSE;
        } 
//        printf("Auto detect eeprom read  %x\n", *svalue);
        if ((*svalue == 0xa55a)||(*svalue == 0xffff)) 
        {
            calData = CALDATA_EEPROM;
//            printf("Calibration Data is in eeprom\n");
            goto Done_auto_storage_detect;
        }
            
        // If eeprom is NOT present on the board then read from eeprom will return zero.
        // We can say a non-zero data of eeprom location 0 means eeprom is present (may be corrupt).
        if (*svalue != 0x0000)
        {
            calData = CALDATA_EEPROM;
//            printf("Calibration Data is in eeprom\n");
            goto Done_auto_storage_detect;
        }
#ifdef MDK_AP          
        {
            int fd;
            unsigned short data;
            if (!((fd = open("/dev/caldata", O_RDWR)) < 0)) {
                read(fd, value, 2);
                data = (value[0]&0xff) | ((value[1]<<8)&0xff00);
                //printf("Auto detect flash read = 0x%x\n", data);
                if ((data == 0xa55a)||(data==0xffff))
                {
                    calData = CALDATA_FLASH;
                  //  printf("Calibration Data is in flash\n");
                    goto Done_auto_storage_detect;
                }
            }
        }
#endif
            // Check for OTP is not implemented yet.
//            printf("TEST OTP\n"); 
    }
    
Done_auto_storage_detect:
//    printf("CalData type = %d\n", calData);

    return calData;
}

#endif  // end of ART_BUILD
#endif

HAL_STATUS
ar9300EepromAttach(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
#ifdef AR9300_EMULATION
	ar9300_eeprom_t *mptr;
	int mdataSize;
#ifdef AR9485_EMULATION    
    systime_t current_system_time = OS_GET_TIMESTAMP();
#endif    
#endif

    ahp->CalibrationDataSource = CalibrationDataNone;
    ahp->CalibrationDataSourceAddress = 0;
    ahp->CalibrationDataTry = CalibrationDataNone;
    ahp->CalibrationDataTryAddress = 0;

    /* To disable searching a type, set its parameter to 0. */
    ahp->TryFromHost=1;
    ahp->TryEeprom=1;
    ahp->TryOtp=1;
#ifdef ATH_CAL_NAND_FLASH
    ahp->TryNand=1;
#else
    ahp->TryFlash=1;
#endif

#ifndef WIN32
    if (ar9300EepDataInFlash(ah)) {
        ar9300FlashMap(ah);
    }
#endif
	/*
	 * ###### This function always return NO SPUR. this is not true for many board designs.
	 * Does anyone use this?
	 */
    AH_PRIVATE(ah)->ah_eepromGetSpurChan = ar9300EepromGetSpurChan;

#ifdef OLDCODE
	// XXX Needs to be moved for dynamic selection
    ahp->ah_eeprom = *(default9300[Ar9300EepromTemplateDefault]);


    if (AR_SREV_HORNET(ah)) {
        /* Set default values for Hornet. */
        ahp->ah_eeprom.baseEepHeader.opCapFlags.opFlags = AR9300_OPFLAGS_11G;
        ahp->ah_eeprom.baseEepHeader.txrxMask = 0x11;
    } else if (AR_SREV_POSEIDON(ah)) {
        /* Set default values for Poseidon. */
        ahp->ah_eeprom.baseEepHeader.opCapFlags.opFlags = AR9300_OPFLAGS_11G;
        ahp->ah_eeprom.baseEepHeader.txrxMask = 0x11;
    }

#ifdef AR9300_EMULATION
    ahp->ah_eeprom.macAddr[0] = 0x00;
    ahp->ah_eeprom.macAddr[1] = 0x03;
    ahp->ah_eeprom.macAddr[2] = 0x7F;
    ahp->ah_eeprom.macAddr[3] = 0xBA;
#ifdef AR9485_EMULATION
    /* Get random MAC to avoid Station address confilct */
    ahp->ah_eeprom.macAddr[4] = (u_int8_t)((current_system_time >> 8) & 0xff);
    ahp->ah_eeprom.macAddr[5] = (u_int8_t)(current_system_time & 0xff);
#else
    ahp->ah_eeprom.macAddr[4] = 0xD0;
    ahp->ah_eeprom.macAddr[5] = 0x00;
#endif
    ahp->ah_emu_eeprom = 1;
    return HAL_OK;
#else
    if (AH_PRIVATE(ah)->ah_config.ath_hal_skipEepromRead) {
		ahp->ah_emu_eeprom = 1;
        return HAL_OK;
	}

    ahp->ah_emu_eeprom = 1;

#ifdef UNUSED
#ifdef ART_BUILD
    ar9300CaldataTypeDetect(ah);
#endif
#endif
    
    if (!ar9300FillEeprom(ah)) {
        return HAL_EIO;
    }

    return HAL_OK;
    //return ar9300CheckEeprom(ah);
#endif
#else
    ahp->ah_emu_eeprom = 1;
#ifdef AR9300_EMULATION
    mptr = &ahp->ah_eeprom;
    mdataSize = ar9300EepromStructSize();
	ar9300EepromRestoreSomething(ah,mptr,mdataSize);
    ahp->ah_eeprom.macAddr[0] = 0x00;
    ahp->ah_eeprom.macAddr[1] = 0x03;
    ahp->ah_eeprom.macAddr[2] = 0x7F;
    ahp->ah_eeprom.macAddr[3] = 0xBA;
#ifdef AR9485_EMULATION
    /* Get random MAC to avoid Station address confilct */
    ahp->ah_eeprom.macAddr[4] = (u_int8_t)((current_system_time >> 8) & 0xff);
    ahp->ah_eeprom.macAddr[5] = (u_int8_t)(current_system_time & 0xff);
#else
    ahp->ah_eeprom.macAddr[4] = 0xD0;
    ahp->ah_eeprom.macAddr[5] = 0x00;
#endif
    return HAL_OK;
#else
#if ATH_DRIVER_SIM
    if (1) { /* Always true but eliminates compilation warning about unreachable code */
        ar9300_eeprom_t *mptr = &ahp->ah_eeprom;
        int mdataSize = ar9300EepromStructSize();
        ar9300EepromRestoreSomething(ah,mptr,mdataSize);
        ahp->ah_eeprom.macAddr[0] = 0x00;
        ahp->ah_eeprom.macAddr[1] = 0x03;
        ahp->ah_eeprom.macAddr[2] = 0x7F;
        ahp->ah_eeprom.macAddr[3] = 0xBA;
        ahp->ah_eeprom.macAddr[4] = 0xD0;
        ahp->ah_eeprom.macAddr[5] = ah->hal_sim.sim_index;
        return HAL_OK;
    }
#endif
#if 0
//#ifdef MDK_AP          //MDK_AP is defined only in NART AP build 
   u_int8_t buffer[10];
   int caldata_check=0;
   
   ar9300CalibrationDataReadFlash(ah, FLASH_BASE_CALDATA_OFFSET, buffer,4);
   printf("flash caldata:: %x\n",buffer[0]);
   if(buffer[0] != 0xff) {
   	caldata_check = 1;
   }
   if (!caldata_check) {
        ar9300_eeprom_t *mptr;
        int mdataSize;
	   if (AR_SREV_HORNET(ah)) {
        /* XXX: For initial testing */

        mptr = &ahp->ah_eeprom;
        mdataSize = ar9300EepromStructSize();
        ahp->ah_eeprom = Ar9300TemplateAP121;
        ahp->ah_emu_eeprom = 1;
        ahp->CalibrationDataSource=CalibrationDataFlash; //need it to let art save into flash ?????
		}
  	else if (AR_SREV_WASP(ah)) {
        /* XXX: For initial testing */

        ath_hal_printf(ah, " wasp eep attach\n");
        mptr = &ahp->ah_eeprom;
        mdataSize = ar9300EepromStructSize();
        ahp->ah_eeprom = Ar9300TemplateGeneric;
        ahp->ah_eeprom.macAddr[0] = 0x00;
        ahp->ah_eeprom.macAddr[1] = 0x03;
        ahp->ah_eeprom.macAddr[2] = 0x7F;
        ahp->ah_eeprom.macAddr[3] = 0xBA;
        ahp->ah_eeprom.macAddr[4] = 0xD0;
        ahp->ah_eeprom.macAddr[5] = 0x00;
        ahp->ah_emu_eeprom = 1;
        ahp->ah_eeprom.baseEepHeader.txrxMask = 0x33;
        ahp->ah_eeprom.baseEepHeader.txrxgain = 0x10;
        ahp->CalibrationDataSource=CalibrationDataFlash; //need it to let art save into flash ?????
    	}
        return HAL_OK;
    }
#endif
    if (AR_SREV_HORNET(ah)) {
        ahp->TryEeprom = 0 ;
    }

    if (!ar9300EepromRestore(ah)) {
        return HAL_EIO;
    }
    return HAL_OK;
#endif
#endif
}

u_int32_t
ar9300EepromGet(struct ath_hal_9300 *ahp, EEPROM_PARAM param)
{
    ar9300_eeprom_t *eep = &ahp->ah_eeprom;
    OSPREY_BASE_EEP_HEADER *pBase = &eep->baseEepHeader;
    OSPREY_BASE_EXTENSION_1 *base_ext1 = &eep->base_ext1;

    switch (param)
    {
#if NOTYET
    case EEP_NFTHRESH_5:
        return pModal[0].noiseFloorThreshCh[0];
    case EEP_NFTHRESH_2:
        return pModal[1].noiseFloorThreshCh[0];
#endif
    case EEP_MAC_LSW:
        return eep->macAddr[0] << 8 | eep->macAddr[1];
    case EEP_MAC_MID:
        return eep->macAddr[2] << 8 | eep->macAddr[3];
    case EEP_MAC_MSW:
        return eep->macAddr[4] << 8 | eep->macAddr[5];
    case EEP_REG_0:
        return pBase->regDmn[0];
    case EEP_REG_1:
        return pBase->regDmn[1];
    case EEP_OP_CAP:
        return pBase->deviceCap;
	case EEP_OP_MODE:
		return pBase->opCapFlags.opFlags;
    case EEP_RF_SILENT:
        return pBase->rfSilent;
#if NOTYET
    case EEP_OB_5:
        return pModal[0].ob;
    case EEP_DB_5:
        return pModal[0].db;
    case EEP_OB_2:
        return pModal[1].ob;
    case EEP_DB_2:
        return pModal[1].db;
    case EEP_MINOR_REV:
        return pBase->eepromVersion & AR9300_EEP_VER_MINOR_MASK;
#endif
    case EEP_TX_MASK:
        return (pBase->txrxMask >> 4) & 0xf;
    case EEP_RX_MASK:
        return pBase->txrxMask & 0xf;
#if NOTYET
    case EEP_FSTCLK_5G:
        return pBase->fastClk5g;
    case EEP_RXGAIN_TYPE:
        return pBase->rxGainType;
#endif
	case EEP_DRIVE_STRENGTH:
#define AR9300_EEP_BASE_DRIVE_STRENGTH	0x1 
        return pBase->miscConfiguration & AR9300_EEP_BASE_DRIVE_STRENGTH;
    case EEP_INTERNAL_REGULATOR:
        /* Bit 4 is internal regulator flag */
        return ((pBase->featureEnable & 0x10) >> 4);
    case EEP_SWREG:
        return (pBase->swreg);
    case EEP_PAPRD_ENABLED:
        /* Bit 5 is papd flag */
       return ((pBase->featureEnable & 0x20) >> 5);
	case EEP_ANTDIV_control:
		return (u_int32_t)(base_ext1->ant_div_control);
    case EEP_CHAIN_MASK_REDUCE:
       return ((pBase->miscConfiguration >> 3) & 0x1);
    case EEP_DEV_TYPE:
        return pBase->deviceType;
    default:
        HALASSERT(0);
        return 0;
}
}

#ifdef AH_SUPPORT_WRITE_EEPROM

/*
 * ar9300EepromSetParam
 */
HAL_BOOL
ar9300EepromSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value)
{
	HAL_BOOL result = AH_TRUE;
#if 0
    struct ath_hal_9300 *ahp = AH9300(ah);
    ar9300_eeprom_t *eep = &ahp->ah_eeprom;
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;
    int offsetRd = 0;
    int offsetChksum = 0;
    u_int16_t checksum;

    offsetRd = AR9300_EEPDEF_START_LOC + (int) (offsetof(struct BaseEepDefHeader, regDmn[0]) >> 1);
    offsetChksum = AR9300_EEPDEF_START_LOC + (offsetof(struct BaseEepDefHeader, checksum) >> 1);

    switch (param) {
    case EEP_REG_0:
	    pBase->regDmn[0] = (u_int16_t) value;

	    result = AH_FALSE;
	    if (ar9300EepromWrite(ah, offsetRd, (u_int16_t) value)) {
		    ar9300EepromDefUpdateChecksum(ahp);
		    checksum = pBase->checksum;
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
		    checksum  = SWAP16(checksum );
#endif
		    if (ar9300EepromWrite(ah, offsetChksum, checksum )) {
			    result = AH_TRUE;
		    }
}
	    break;
    default:
	    HALASSERT(0);
	    break;
	}
#endif
    return result;
}
#endif //#ifdef AH_SUPPORT_WRITE_EEPROM


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
ar9300INIFixup(struct ath_hal *ah,ar9300_eeprom_t *pEepData, u_int32_t reg, u_int32_t value)
{
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "ar9300EepromDefINIFixup: FIXME\n");
#if 0
    BASE_EEPDEF_HEADER  *pBase  = &(pEepData->baseEepHeader);

    switch (AH_PRIVATE(ah)->ah_devid)
    {
    case AR9300_DEVID_AR9300_PCI:
        /*
        ** Need to set the external/internal regulator bit to the proper value.
        ** Can only write this ONCE.
        */

        if ( reg == 0x7894 )
        {
            /*
            ** Check for an EEPROM data structure of "0x0b" or better
            */

            HDPRINTF(ah, HAL_DBG_EEPROM, "ini VAL: %x  EEPROM: %x\n",
                     value,(pBase->version & 0xff));

            if ( (pBase->version & 0xff) > 0x0a) {
                HDPRINTF(ah, HAL_DBG_EEPROM, "PWDCLKIND: %d\n",pBase->pwdclkind);
                value &= ~AR_AN_TOP2_PWDCLKIND;
                value |= AR_AN_TOP2_PWDCLKIND & ( pBase->pwdclkind <<  AR_AN_TOP2_PWDCLKIND_S);
            } else {
                HDPRINTF(ah, HAL_DBG_EEPROM, "PWDCLKIND Earlier Rev\n");
            }

            HDPRINTF(ah, HAL_DBG_EEPROM, "final ini VAL: %x\n", value);
        }
        break;

    }

    return (value);
#else
    return 0;
#endif
}

/*
 * Returns the interpolated y value corresponding to the specified x value
 * from the np ordered pairs of data (px,py).
 * The pairs do not have to be in any order.
 * If the specified x value is less than any of the px,
 * the returned y value is equal to the py for the lowest px.
 * If the specified x value is greater than any of the px,
 * the returned y value is equal to the py for the highest px.
 */
static int
interpolate(int32_t x, int32_t *px, int32_t *py, u_int16_t np)
{
    int ip = 0;
    int lx = 0,ly = 0,lhave = 0;
    int hx = 0,hy = 0,hhave = 0;
    int dx = 0;
    int y = 0;
	int bf,factor,plus;

    lhave=0;
    hhave=0;
    //
    // identify best lower and higher x calibration measurement
    //
    for (ip=0; ip<np; ip++)
    {
        dx=x-px[ip];
        //
        // this measurement is higher than our desired x
        //
        if (dx<=0)
        {
            if (!hhave || dx>(x-hx))
            {
                //
                // new best higher x measurement
                //
                hx=px[ip];
                hy=py[ip];
                hhave=1;
            }
        }
        //
        // this measurement is lower than our desired x
        //
        if (dx>=0)
        {
            if (!lhave || dx<(x-lx))
            {
                //
                // new best lower x measurement
                //
                lx=px[ip];
                ly=py[ip];
                lhave=1;
            }
        }
    }
    //
    // the low x is good
    //
    if (lhave)
    {
        //
        // so is the high x
        //
        if (hhave)
        {
            //
            // they're the same, so just pick one
            //
            if (hx==lx)
            {
                y=ly;
            }
            //
            // interpolate with round off
            //
            else
            {
                bf=(2*(hy-ly)*(x-lx))/(hx-lx);
				plus=(bf%2);
				factor=bf/2;
                y=ly+factor+plus;
            }
        }
        //
        // only low is good, use it
        //
        else
        {
            y=ly;
        }
    }
    //
    // only high is good, use it
    //
    else if (hhave)
    {
        y=hy;
    }
    //
    // nothing is good,this should never happen unless np=0, ????
    //
    else
    {
        y= -(1<<30);
    }

    return y;
}

u_int8_t
ar9300EepromGetLegacyTrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq, HAL_BOOL is2GHz)
{
    u_int16_t numPiers, i;
    int32_t targetPowerArray[OSPREY_NUM_5G_20_TARGET_POWERS];
    int32_t freqArray[OSPREY_NUM_5G_20_TARGET_POWERS]; 
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
    CAL_TARGET_POWER_LEG *pEepromTargetPwr;
	u_int8_t *pFreqBin;

    if (is2GHz) {
        numPiers = OSPREY_NUM_2G_20_TARGET_POWERS;    
        pEepromTargetPwr = eep->calTargetPower2G;
		pFreqBin = eep->calTarget_freqbin_2G;
    } else {
        numPiers = OSPREY_NUM_5G_20_TARGET_POWERS;
        pEepromTargetPwr = eep->calTargetPower5G;
 		pFreqBin = eep->calTarget_freqbin_5G;
   }

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
		freqArray[i] = FBIN2FREQ(pFreqBin[i], is2GHz);
        targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
    }

    //interpolate to get target power for given frequency
    return((u_int8_t)interpolate((int32_t)freq, freqArray, targetPowerArray, numPiers));
}

u_int8_t
ar9300EepromGetHT20TrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq, HAL_BOOL is2GHz)
{
    u_int16_t numPiers, i;
    int32_t targetPowerArray[OSPREY_NUM_5G_20_TARGET_POWERS];
    int32_t freqArray[OSPREY_NUM_5G_20_TARGET_POWERS]; 
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
    OSP_CAL_TARGET_POWER_HT *pEepromTargetPwr;
	u_int8_t *pFreqBin;

    if (is2GHz) {
        numPiers = OSPREY_NUM_2G_20_TARGET_POWERS;    
        pEepromTargetPwr = eep->calTargetPower2GHT20;
		pFreqBin = eep->calTarget_freqbin_2GHT20;
    } else {
        numPiers = OSPREY_NUM_5G_20_TARGET_POWERS;
        pEepromTargetPwr = eep->calTargetPower5GHT20;
		pFreqBin = eep->calTarget_freqbin_5GHT20;
    }

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
		freqArray[i] = FBIN2FREQ(pFreqBin[i], is2GHz);
        targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
    }

    //interpolate to get target power for given frequency
    return((u_int8_t)interpolate((int32_t)freq, freqArray, targetPowerArray, numPiers));
}

u_int8_t
ar9300EepromGetHT40TrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq, HAL_BOOL is2GHz)
{
    u_int16_t numPiers, i;
    int32_t targetPowerArray[OSPREY_NUM_5G_40_TARGET_POWERS];
    int32_t freqArray[OSPREY_NUM_5G_40_TARGET_POWERS]; 
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
    OSP_CAL_TARGET_POWER_HT *pEepromTargetPwr;
	u_int8_t *pFreqBin;

    if (is2GHz) {
        numPiers = OSPREY_NUM_2G_40_TARGET_POWERS;    
        pEepromTargetPwr = eep->calTargetPower2GHT40;
		pFreqBin = eep->calTarget_freqbin_2GHT40;
    } else {
        numPiers = OSPREY_NUM_5G_40_TARGET_POWERS;
        pEepromTargetPwr = eep->calTargetPower5GHT40;
 		pFreqBin = eep->calTarget_freqbin_5GHT40;
   }

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
 		freqArray[i] = FBIN2FREQ(pFreqBin[i], is2GHz);
        targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
    }

    //interpolate to get target power for given frequency
    return((u_int8_t)interpolate((int32_t)freq, freqArray, targetPowerArray, numPiers));
}

u_int8_t
ar9300EepromGetCckTrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq)
{
    u_int16_t numPiers = OSPREY_NUM_2G_CCK_TARGET_POWERS, i;
    int32_t targetPowerArray[OSPREY_NUM_2G_CCK_TARGET_POWERS];
    int32_t freqArray[OSPREY_NUM_2G_CCK_TARGET_POWERS]; 
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
    CAL_TARGET_POWER_LEG *pEepromTargetPwr = eep->calTargetPowerCck;
	u_int8_t *pFreqBin=eep->calTarget_freqbin_Cck;

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
  		freqArray[i] = FBIN2FREQ(pFreqBin[i], 1);
        targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
    }

    //interpolate to get target power for given frequency
    return((u_int8_t)interpolate((int32_t)freq, freqArray, targetPowerArray, numPiers));
}

//
// Set tx power registers to array of values passed in
//
int
ar9300TransmitPowerRegWrite(struct ath_hal *ah, u_int8_t *pPwrArray) 
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))
	/* make sure forced gain is not set */
    //FieldWrite("force_dac_gain", 0);
    //OS_REG_WRITE(ah, 0xa3f8, 0);
	//FieldWrite("force_tx_gain", 0);
    
    OS_REG_WRITE(ah, 0xa458, 0);

    /* Write the OFDM power per rate set */
    /* 6 (LSB), 9, 12, 18 (MSB) */
    OS_REG_WRITE(ah, 0xa3c0,
        POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 24)
          | POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 16)
          | POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24],  8)
          | POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24],  0)
    );
    /* 24 (LSB), 36, 48, 54 (MSB) */
    OS_REG_WRITE(ah, 0xa3c4,
        POW_SM(pPwrArray[ALL_TARGET_LEGACY_54], 24)
          | POW_SM(pPwrArray[ALL_TARGET_LEGACY_48], 16)
          | POW_SM(pPwrArray[ALL_TARGET_LEGACY_36],  8)
          | POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24],  0)
    );

	/* Write the CCK power per rate set */
    /* 1L (LSB), reserved, 2L, 2S (MSB) */  
	OS_REG_WRITE(ah, 0xa3c8,
	    POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L], 24)
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L],  16)
//		  | POW_SM(txPowerTimes2,  8) /* this is reserved for Osprey */
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L],   0)
	);
    /* 5.5L (LSB), 5.5S, 11L, 11S (MSB) */
	OS_REG_WRITE(ah, 0xa3cc,
	    POW_SM(pPwrArray[ALL_TARGET_LEGACY_11S], 24)
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_11L], 16)
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_5S],  8)
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L],  0)
	);

	/* write the power for duplicated frames - HT40 */
	/* dup40_cck (LSB), dup40_ofdm, ext20_cck, ext20_ofdm  (MSB) */
	OS_REG_WRITE(ah, 0xa3e0,
	    POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24], 24)
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L], 16)
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_6_24],  8)
		  | POW_SM(pPwrArray[ALL_TARGET_LEGACY_1L_5L],  0)
	);

    /* Write the HT20 power per rate set */
    /* 0/8/16 (LSB), 1-3/9-11/17-19, 4, 5 (MSB) */
    OS_REG_WRITE(ah, 0xa3d0,
        POW_SM(pPwrArray[ALL_TARGET_HT20_5], 24)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_4],  16)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_1_3_9_11_17_19],  8)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_0_8_16],   0)
    );
    
    /* 6 (LSB), 7, 12, 13 (MSB) */
    OS_REG_WRITE(ah, 0xa3d4,
        POW_SM(pPwrArray[ALL_TARGET_HT20_13], 24)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_12],  16)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_7],  8)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_6],   0)
    );

    /* 14 (LSB), 15, 20, 21 */
    OS_REG_WRITE(ah, 0xa3e4,
        POW_SM(pPwrArray[ALL_TARGET_HT20_21], 24)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_20],  16)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_15],  8)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_14],   0)
    );

    /* Mixed HT20 and HT40 rates */
    /* HT20 22 (LSB), HT20 23, HT40 22, HT40 23 (MSB) */
    OS_REG_WRITE(ah, 0xa3e8,
        POW_SM(pPwrArray[ALL_TARGET_HT40_23], 24)
          | POW_SM(pPwrArray[ALL_TARGET_HT40_22],  16)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_23],  8)
          | POW_SM(pPwrArray[ALL_TARGET_HT20_22],   0)
    );
    
    /* Write the HT40 power per rate set */
    // correct PAR difference between HT40 and HT20/LEGACY
    /* 0/8/16 (LSB), 1-3/9-11/17-19, 4, 5 (MSB) */
    OS_REG_WRITE(ah, 0xa3d8,
        POW_SM(pPwrArray[ALL_TARGET_HT40_5], 24)
          | POW_SM(pPwrArray[ALL_TARGET_HT40_4],  16)
          | POW_SM(pPwrArray[ALL_TARGET_HT40_1_3_9_11_17_19],  8)
          | POW_SM(pPwrArray[ALL_TARGET_HT40_0_8_16],   0)
    );

    /* 6 (LSB), 7, 12, 13 (MSB) */
    OS_REG_WRITE(ah, 0xa3dc,
        POW_SM(pPwrArray[ALL_TARGET_HT40_13], 24)
          | POW_SM(pPwrArray[ALL_TARGET_HT40_12],  16)
          | POW_SM(pPwrArray[ALL_TARGET_HT40_7], 8)
          | POW_SM(pPwrArray[ALL_TARGET_HT40_6], 0)
    );

    /* 14 (LSB), 15, 20, 21 */
	OS_REG_WRITE(ah, 0xa3ec,
	    POW_SM(pPwrArray[ALL_TARGET_HT40_21], 24)
	      | POW_SM(pPwrArray[ALL_TARGET_HT40_20],  16)
	      | POW_SM(pPwrArray[ALL_TARGET_HT40_15],  8)
	      | POW_SM(pPwrArray[ALL_TARGET_HT40_14],   0)
	);

	return 0;
#undef POW_SM    
}

void
ar9300_selfgen_tpc_reg_write(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
                             u_int8_t *p_pwr_array)
{
    u_int32_t tpc_reg_val;

    /* Set the target power values for self generated frames (ACK,RTS/CTS) to
     * be within limits. This is just a safety measure.With per packet TPC mode
     * enabled the target power value used with self generated frames will be
     * MIN( TPC reg, BB_powertx_rate register)
     */

    if (IS_CHAN_2GHZ(chan)) {
        tpc_reg_val = (SM(p_pwr_array[ALL_TARGET_LEGACY_1L_5L], AR_TPC_ACK) |
                       SM(p_pwr_array[ALL_TARGET_LEGACY_1L_5L], AR_TPC_CTS) |
                       SM(0x3f, AR_TPC_CHIRP) |
                       SM(0x3f, AR_TPC_RPT));
    } else {
        tpc_reg_val = (SM(p_pwr_array[ALL_TARGET_LEGACY_6_24], AR_TPC_ACK) |
                       SM(p_pwr_array[ALL_TARGET_LEGACY_6_24], AR_TPC_CTS) |
                       SM(0x3f, AR_TPC_CHIRP) |
                       SM(0x3f, AR_TPC_RPT));
    }
    OS_REG_WRITE(ah, AR_TPC, tpc_reg_val);
}

void
ar9300SetTargetPowerFromEeprom(struct ath_hal *ah, u_int16_t freq, u_int8_t *targetPowerValT2)
{
    u_int8_t ht40PowerIncForPdadc = 0; //hard code for now, need to get from eeprom struct
    HAL_BOOL  is2GHz = 0;
    

    if (freq < 4000) {
        is2GHz = 1;
    }

    targetPowerValT2[ALL_TARGET_LEGACY_6_24] = ar9300EepromGetLegacyTrgtPwr(ah, LEGACY_TARGET_RATE_6_24, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_LEGACY_36] = ar9300EepromGetLegacyTrgtPwr(ah, LEGACY_TARGET_RATE_36, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_LEGACY_48] = ar9300EepromGetLegacyTrgtPwr(ah, LEGACY_TARGET_RATE_48, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_LEGACY_54] = ar9300EepromGetLegacyTrgtPwr(ah, LEGACY_TARGET_RATE_54, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_LEGACY_1L_5L] = ar9300EepromGetCckTrgtPwr(ah, LEGACY_TARGET_RATE_1L_5L, freq);
    targetPowerValT2[ALL_TARGET_LEGACY_5S] = ar9300EepromGetCckTrgtPwr(ah, LEGACY_TARGET_RATE_5S, freq);
    targetPowerValT2[ALL_TARGET_LEGACY_11L] = ar9300EepromGetCckTrgtPwr(ah, LEGACY_TARGET_RATE_11L, freq);
    targetPowerValT2[ALL_TARGET_LEGACY_11S] = ar9300EepromGetCckTrgtPwr(ah, LEGACY_TARGET_RATE_11S, freq);
    targetPowerValT2[ALL_TARGET_HT20_0_8_16] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_0_8_16, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_1_3_9_11_17_19] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_1_3_9_11_17_19, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_4] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_4, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_5] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_5, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_6] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_6, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_7] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_7, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_12] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_12, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_13] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_13, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_14] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_14, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_15] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_15, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_20] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_20, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_21] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_21, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_22] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_22, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT20_23] = ar9300EepromGetHT20TrgtPwr(ah, HT_TARGET_RATE_23, freq, is2GHz);
    targetPowerValT2[ALL_TARGET_HT40_0_8_16] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_0_8_16, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_1_3_9_11_17_19] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_1_3_9_11_17_19, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_4] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_4, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_5] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_5, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_6] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_6, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_7] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_7, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_12] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_12, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_13] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_13, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_14] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_14, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_15] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_15, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_20] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_20, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_21] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_21, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_22] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_22, freq, is2GHz) + ht40PowerIncForPdadc;
    targetPowerValT2[ALL_TARGET_HT40_23] = ar9300EepromGetHT40TrgtPwr(ah, HT_TARGET_RATE_23, freq, is2GHz) + ht40PowerIncForPdadc;

    {
        int  i = 0;

        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: APPLYING TARGET POWERS\n", __func__);
        while( i < ar9300RateSize)
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: TPC[%02d] 0x%08x ",
					 __func__, i, targetPowerValT2[i]);
            i++;
			if (i == ar9300RateSize) {
				break;
			}
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: TPC[%02d] 0x%08x ",
					 __func__, i, targetPowerValT2[i]);
            i++;
			if (i == ar9300RateSize) {
				break;
			}
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: TPC[%02d] 0x%08x ",
					 __func__, i, targetPowerValT2[i]);
            i++;
			if (i == ar9300RateSize) {
				break;
			}
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: TPC[%02d] 0x%08x \n",
					 __func__, i, targetPowerValT2[i]);
            i++;
        }
    }
} 

u_int16_t *ar9300RegulatoryDomainGet(struct ath_hal *ah)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	return eep->baseEepHeader.regDmn;
}


int32_t 
ar9300EepromWriteEnableGpioGet(struct ath_hal *ah)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	return eep->baseEepHeader.eepromWriteEnableGpio;
}

int32_t 
ar9300WlanDisableGpioGet(struct ath_hal *ah)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	return eep->baseEepHeader.wlanDisableGpio;
}

int32_t 
ar9300WlanLedGpioGet(struct ath_hal *ah)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	return eep->baseEepHeader.wlanLedGpio;
}

int32_t 
ar9300RxBandSelectGpioGet(struct ath_hal *ah)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	return eep->baseEepHeader.rxBandSelectGpio;
}

/*
 * since valid noise floor values are negative, returns 1 on error
 */
int32_t
ar9300NoiseFloorCalOrPowerGet(struct ath_hal *ah, int32_t frequency,
    int32_t ichain, HAL_BOOL use_cal)
{
    int nfuse = 1; /* start with an error return value */
    int32_t fx[OSPREY_NUM_5G_CAL_PIERS+OSPREY_NUM_2G_CAL_PIERS];
    int32_t nf[OSPREY_NUM_5G_CAL_PIERS+OSPREY_NUM_2G_CAL_PIERS];
    int nnf;
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
    u_int8_t *pCalPier;
    OSP_CAL_DATA_PER_FREQ_OP_LOOP *pCalPierStruct;
    int is2GHz;
    int ipier, npier;

    /*
     * check chain value
     */
    if (ichain<0 || ichain>=OSPREY_MAX_CHAINS) {
        return 1;
    }

    /* figure out which band we're using */
    is2GHz = (frequency < 4000);
    if (is2GHz) {
        npier = OSPREY_NUM_2G_CAL_PIERS;
        pCalPier = eep->calFreqPier2G;
        pCalPierStruct = eep->calPierData2G[ichain];
    } else {
        npier = OSPREY_NUM_5G_CAL_PIERS;
        pCalPier = eep->calFreqPier5G;
        pCalPierStruct = eep->calPierData5G[ichain];
    }
    /* look for valid noise floor values */
    nnf = 0;
    for (ipier=0; ipier<npier; ipier++) {
        fx[nnf] = FBIN2FREQ(pCalPier[ipier], is2GHz);
        nf[nnf] = use_cal ?
            pCalPierStruct[ipier].rxNoisefloorCal :
            pCalPierStruct[ipier].rxNoisefloorPower;
        if (nf[nnf] < 0) {
            nnf++;
        }
    }
    /*
     * If we have some valid values, interpolate to find the value
     * at the desired frequency.
     */
    if (nnf > 0) {
        nfuse = interpolate(frequency, fx, nf, nnf);
    }

    return nfuse;
}

int32_t ar9300RxGainIndexGet(struct ath_hal *ah)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;

    return (eep->baseEepHeader.txrxgain)&0xf;		// bits 3:0
}


int32_t ar9300TxGainIndexGet(struct ath_hal *ah)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;

    return (eep->baseEepHeader.txrxgain>>4)&0xf;	// bits 7:4
}

HAL_BOOL ar9300InternalRegulatorApply(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int internal_regulator = ar9300EepromGet(ahp, EEP_INTERNAL_REGULATOR);
    int reg_pmu1,reg_pmu2,reg_pmu1_set,reg_pmu2_set;
    int time_out = 100;
    unsigned long eep_addr;
    u_int32_t reg_val, reg_usb = 0, reg_pmu = 0;
    int usb_valid = 0, pmu_valid = 0;
    unsigned char pmu_refv; 

    if (internal_regulator) {
        if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
            if (AR_SREV_HORNET(ah)) {
                /* Read OTP first */
                for (eep_addr = 0x14; ; eep_addr -= 0x10) {

                    ar9300OtpRead(ah, eep_addr / 4, &reg_val);

                    if ((reg_val & 0x80) == 0x80){
                        usb_valid = 1;
                        reg_usb = reg_val & 0x000000ff;
                    }
                    
                    if ((reg_val & 0x80000000) == 0x80000000){
                        pmu_valid = 1;
                        reg_pmu = (reg_val & 0xff000000) >> 24;
                    }

                    if (eep_addr == 0x4) {
                        break;
                    }
                }

                if (pmu_valid) {
                    pmu_refv = reg_pmu & 0xf;
                } else {
                    pmu_refv = 0x8;
                }

                /*
                 * If (valid) {
                 *   Usb_phy_ctrl2_tx_cal_en -> 0
                 *   Usb_phy_ctrl2_tx_cal_sel -> 0
                 *   Usb_phy_ctrl2_tx_man_cal -> 0, 1, 3, 7 or 15 from OTP
                 * }
                 */
                if (usb_valid) {
                    OS_REG_RMW_FIELD(ah, 0x16c88, AR_PHY_CTRL2_TX_CAL_EN, 0x0);
                    OS_REG_RMW_FIELD(ah, 0x16c88, AR_PHY_CTRL2_TX_CAL_SEL, 0x0);
                    OS_REG_RMW_FIELD(ah, 0x16c88, 
                        AR_PHY_CTRL2_TX_MAN_CAL, (reg_usb & 0xf));
                }

            } else {
                pmu_refv = 0x8;
            }
           //#ifndef USE_HIF
           /* Follow the MDK settings for Hornet PMU.
             * my $pwd             = 0x0;
             * my $Nfdiv           = 0x3;  # xtal_freq = 25MHz
             * my $Nfdiv           = 0x4;  # xtal_freq = 40MHz
             * my $Refv            = 0x7;  # 0x5:1.22V; 0x8:1.29V
             * my $Gm1             = 0x3;  #Poseidon $Gm1=1
             * my $classb          = 0x0;
             * my $Cc              = 0x1;  #Poseidon $Cc=7
             * my $Rc              = 0x6;
             * my $RampSlope       = 0x1;
             * my $Segm            = 0x3;
             * my $UseLocalOsc     = 0x0;
             * my $ForceXoscStable = 0x0;
             * my $Selfb           = 0x0;  #Poseidon $Selfb=1
             * my $Filterfb        = 0x3;  #Poseidon $Filterfb=0
             * my $Filtervc        = 0x0;
             * my $disc            = 0x0;
             * my $discdel         = 0x4;
             * my $spare           = 0x0;
             * $reg_PMU1 = $pwd | ($Nfdiv<<1) | ($Refv<<4) | ($Gm1<<8) |
             *             ($classb<<11) | ($Cc<<14) | ($Rc<<17) | ($RampSlope<<20) |
             *             ($Segm<<24) | ($UseLocalOsc<<26) | ($ForceXoscStable<<27) |
             *             ($Selfb<<28) | ($Filterfb<<29) ;
             * $reg_PMU2 = $handle->regRd("ch0_PMU2");
             * $reg_PMU2 = ($reg_PMU2 & 0xfe3fffff) | ($Filtervc<<22) ;
             * $reg_PMU2 = ($reg_PMU2 & 0xe3ffffff) | ($discdel<<26) ;
             * $reg_PMU2 = ($reg_PMU2 & 0x1fffffff) | ($spare<<29) ; 
             */
             if (ahp->clk_25mhz) {
				reg_pmu1_set = 0 | (3<<1) | (pmu_refv<<4) | (3<<8) |
                               (0<<11) | (1<<14) | (6<<17) | (1<<20) |
                               (3<<24) | (0<<26) | (0<<27) |
                               (0<<28) | (0<<29) ;
             } else {
                if (AR_SREV_POSEIDON(ah)) {
                    reg_pmu1_set = 0 | 
                                (5<<1)  | (7<<4)  | (2<<8)  | (0<<11) |
                                (2<<14) | (6<<17) | (1<<20) | (3<<24) |
                                (0<<26) | (0<<27) | (1<<28) | (0<<29) ;
                } else {
                    reg_pmu1_set = 0 |
                                (4<<1)  | (7<<4)  | (3<<8)  | (0<<11) |
                                (1<<14) | (6<<17) | (1<<20) | (3<<24) |
                                (0<<26) | (0<<27) | (0<<28) | (0<<29) ;
                                
                } 
             }
             reg_pmu2_set = (OS_REG_READ(ah, AR_PHY_PMU2) & (~AR_PHY_PMU2_PGM));
             OS_REG_WRITE(ah, AR_PHY_PMU2, reg_pmu2_set);
             reg_pmu2 = OS_REG_READ(ah, AR_PHY_PMU2);
             time_out = 100;
             while(reg_pmu2 != reg_pmu2_set) {
             	if(time_out-- == 0)
             	{
             		return AH_FALSE;
             	}
                OS_REG_WRITE(ah, AR_PHY_PMU2, reg_pmu2_set);
                OS_DELAY(10);
                reg_pmu2 = OS_REG_READ(ah, AR_PHY_PMU2);
             }
             
             OS_REG_WRITE(ah, AR_PHY_PMU1, reg_pmu1_set);   //0x638c8376
             reg_pmu1 = OS_REG_READ(ah, AR_PHY_PMU1);
             time_out = 100;
             while(reg_pmu1 != reg_pmu1_set) {
             	if(time_out-- == 0)
             	{
             		return AH_FALSE;
             	}
                OS_REG_WRITE(ah, AR_PHY_PMU1, reg_pmu1_set);   //0x638c8376
                OS_DELAY(10);
                reg_pmu1 = OS_REG_READ(ah, AR_PHY_PMU1);
             }
                                
             reg_pmu2_set = (OS_REG_READ(ah, AR_PHY_PMU2) & (~0xFFC00000)) | (4<<26);
             OS_REG_WRITE(ah, AR_PHY_PMU2, reg_pmu2_set);
             reg_pmu2 = OS_REG_READ(ah, AR_PHY_PMU2);
             time_out = 100;
             while(reg_pmu2 != reg_pmu2_set) {
             	if(time_out-- == 0)
             	{
             		return AH_FALSE;
             	}
                OS_REG_WRITE(ah, AR_PHY_PMU2, reg_pmu2_set);
                OS_DELAY(10);
                reg_pmu2 = OS_REG_READ(ah, AR_PHY_PMU2);
             }
             reg_pmu2_set = (OS_REG_READ(ah, AR_PHY_PMU2) & (~AR_PHY_PMU2_PGM)) | (1<<21);
             OS_REG_WRITE(ah, AR_PHY_PMU2, reg_pmu2_set);
             reg_pmu2 = OS_REG_READ(ah, AR_PHY_PMU2);
             time_out = 100;
             while(reg_pmu2 != reg_pmu2_set) {
             	if(time_out-- == 0)
             	{
             		return AH_FALSE;
             	}
                OS_REG_WRITE(ah, AR_PHY_PMU2, reg_pmu2_set);
                OS_DELAY(10);
                reg_pmu2 = OS_REG_READ(ah, AR_PHY_PMU2);
             }

            //#endif
        }else {
            /* Internal regulator is ON. Write swreg register. */
            int swreg = ar9300EepromGet(ahp, EEP_SWREG);
            OS_REG_WRITE(ah, AR_RTC_REG_CONTROL1,
                         OS_REG_READ(ah, AR_RTC_REG_CONTROL1) &
                         (~AR_RTC_REG_CONTROL1_SWREG_PROGRAM));
            OS_REG_WRITE(ah, AR_RTC_REG_CONTROL0, swreg);
            /* Set REG_CONTROL1.SWREG_PROGRAM */
            OS_REG_WRITE(ah, AR_RTC_REG_CONTROL1,
                         OS_REG_READ(ah,AR_RTC_REG_CONTROL1) |
                         AR_RTC_REG_CONTROL1_SWREG_PROGRAM);
        }
    } else {
        if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
            OS_REG_RMW_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM, 0x0);
            reg_pmu2 = OS_REG_READ_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM);
            while(reg_pmu2) {
                OS_DELAY(10);
                reg_pmu2 = OS_REG_READ_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM);
            }
            OS_REG_RMW_FIELD(ah, AR_PHY_PMU1, AR_PHY_PMU1_PWD, 0x1);
            reg_pmu1 = OS_REG_READ_FIELD(ah, AR_PHY_PMU1, AR_PHY_PMU1_PWD);
            while(!reg_pmu1) {
                OS_DELAY(10);
                reg_pmu1 = OS_REG_READ_FIELD(ah, AR_PHY_PMU1, AR_PHY_PMU1_PWD);
            }
            OS_REG_RMW_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM, 0x1);
            reg_pmu2 = OS_REG_READ_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM);
            while(!reg_pmu2) {
                OS_DELAY(10);
                reg_pmu2 = OS_REG_READ_FIELD(ah, AR_PHY_PMU2, AR_PHY_PMU2_PGM);
            }
        } else {
            OS_REG_WRITE(ah, AR_RTC_SLEEP_CLK,
                         (OS_REG_READ(ah, AR_RTC_SLEEP_CLK)|
                          AR_RTC_FORCE_SWREG_PRD|AR_RTC_PCIE_RST_PWDN_EN));
        }
    }

    return AH_TRUE;  
}


HAL_BOOL ar9300DriveStrengthApply(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
	int drive_strength;
	unsigned long reg;

	drive_strength = ar9300EepromGet(ahp, EEP_DRIVE_STRENGTH);
	if (drive_strength) {
        reg = OS_REG_READ(ah, AR_PHY_65NM_CH0_BIAS1);
		reg &= ~0x00ffffc0;
		reg |= 0x5 << 21;
		reg |= 0x5 << 18;
		reg |= 0x5 << 15;
		reg |= 0x5 << 12;
		reg |= 0x5 << 9;
		reg |= 0x5 << 6;
        OS_REG_WRITE(ah, AR_PHY_65NM_CH0_BIAS1, reg);

        reg = OS_REG_READ(ah, AR_PHY_65NM_CH0_BIAS2);
		reg &= ~0xffffffe0;
		reg |= 0x5 << 29;
		reg |= 0x5 << 26;
		reg |= 0x5 << 23;
		reg |= 0x5 << 20;
		reg |= 0x5 << 17;
		reg |= 0x5 << 14;
		reg |= 0x5 << 11;
		reg |= 0x5 << 8;
		reg |= 0x5 << 5;
        OS_REG_WRITE(ah, AR_PHY_65NM_CH0_BIAS2, reg);

        reg = OS_REG_READ(ah, AR_PHY_65NM_CH0_BIAS4);
		reg &= ~0xff800000;
		reg |= 0x5 << 29;
		reg |= 0x5 << 26;
		reg |= 0x5 << 23;
        OS_REG_WRITE(ah, AR_PHY_65NM_CH0_BIAS4, reg);
	}
	return 0;
}

int32_t ar9300XpaBiasLevelGet(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	if (is2GHz)
	{
		return eep->modalHeader2G.xpaBiasLvl;
	}
	else
	{
		return eep->modalHeader5G.xpaBiasLvl;
	}
}

HAL_BOOL ar9300XpaBiasLevelApply(struct ath_hal *ah, HAL_BOOL is2GHz)
{
/* In ar9330 emu, we can't access radio registers, 
 * merlin is used for radio part.
 */
#if !defined(AR9330_EMULATION) && !defined(AR9485_EMULATION)

	int bias;
    bias = ar9300XpaBiasLevelGet(ah, is2GHz);

    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_WASP(ah)) {
        OS_REG_RMW_FIELD(
            ah, AR_HORNET_CH0_TOP2, AR_HORNET_CH0_TOP2_XPABIASLVL, bias);
    } else {
        OS_REG_RMW_FIELD(
            ah, AR_PHY_65NM_CH0_TOP, AR_PHY_65NM_CH0_TOP_XPABIASLVL, bias);
        OS_REG_RMW_FIELD(
            ah, AR_PHY_65NM_CH0_THERM, AR_PHY_65NM_CH0_THERM_XPABIASLVL_MSB,
            bias >> 2);
        OS_REG_RMW_FIELD(
            ah, AR_PHY_65NM_CH0_THERM, AR_PHY_65NM_CH0_THERM_XPASHORT2GND, 1);
    }
#endif
	return 0;
}

u_int32_t ar9300AntCtrlCommonGet(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	if (is2GHz)
	{
		return eep->modalHeader2G.antCtrlCommon;
	}
	else
	{
		return eep->modalHeader5G.antCtrlCommon;
	}
}

u_int32_t ar9300AntCtrlCommon2Get(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	if (is2GHz)
	{
		return eep->modalHeader2G.antCtrlCommon2;
	}
	else
	{
		return eep->modalHeader5G.antCtrlCommon2;
	}
}

u_int16_t ar9300AntCtrlChainGet(struct ath_hal *ah, int chain, HAL_BOOL is2GHz)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	if (chain>=0 && chain<OSPREY_MAX_CHAINS)
	{
		if (is2GHz)
		{
			return eep->modalHeader2G.antCtrlChain[chain];
		}
		else
		{
			return eep->modalHeader5G.antCtrlChain[chain];
		}
	}
	return 0;
}


HAL_BOOL ar9300AntCtrlApply(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    u_int32_t value;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t regval;
#if ATH_ANT_DIV_COMB 
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pcap = &ahpriv->ah_caps;
#endif  /* ATH_ANT_DIV_COMB */

#define AR_SWITCH_TABLE_COM_ALL (0xffff)
#define AR_SWITCH_TABLE_COM_ALL_S (0)
	value=ar9300AntCtrlCommonGet(ah, is2GHz);
    OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_COM, AR_SWITCH_TABLE_COM_ALL, value);

#define AR_SWITCH_TABLE_COM2_ALL (0xffffff)
#define AR_SWITCH_TABLE_COM2_ALL_S (0)
	value=ar9300AntCtrlCommon2Get(ah, is2GHz);
    OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_COM_2, AR_SWITCH_TABLE_COM2_ALL, value);

#define AR_SWITCH_TABLE_ALL (0xfff)
#define AR_SWITCH_TABLE_ALL_S (0)
	value=ar9300AntCtrlChainGet(ah, 0, is2GHz);
    OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN_0, AR_SWITCH_TABLE_ALL, value);

    if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
        value=ar9300AntCtrlChainGet(ah, 1, is2GHz);
        OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN_1, AR_SWITCH_TABLE_ALL, value);

        if (!AR_SREV_WASP(ah)) {
            value=ar9300AntCtrlChainGet(ah, 2, is2GHz);
            OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN_2, AR_SWITCH_TABLE_ALL, value);
        }
    }
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
        value = ar9300EepromGet(ahp, EEP_ANTDIV_control);
        /* main_lnaconf, alt_lnaconf, main_tb, alt_tb */
        regval = OS_REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
        regval &= (~ANT_DIV_CONTROL_ALL); /* clear bit 25~30 */     
        regval |= (value & 0x3f) << ANT_DIV_CONTROL_ALL_S; 
        /* enable_lnadiv */
        regval &= (~MULTICHAIN_GAIN_CTRL__ENABLE_ANT_DIV_LNADIV__MASK);
        regval |= ((value >> 6) & 0x1) << 
                  MULTICHAIN_GAIN_CTRL__ENABLE_ANT_DIV_LNADIV__SHIFT; 
        OS_REG_WRITE(ah, AR_PHY_MC_GAIN_CTRL, regval);
        
        /* enable fast_div */
        regval = OS_REG_READ(ah, AR_PHY_CCK_DETECT);
        regval &= (~BBB_SIG_DETECT__ENABLE_ANT_FAST_DIV__MASK);
        regval |= ((value >> 7) & 0x1) << 
                  BBB_SIG_DETECT__ENABLE_ANT_FAST_DIV__SHIFT;
        OS_REG_WRITE(ah, AR_PHY_CCK_DETECT, regval);        
    }

#if ATH_ANT_DIV_COMB    
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON_11(ah)) {
        if (pcap->halAntDivCombSupport) {
            /* If support DivComb, set MAIN to LNA1 and ALT to LNA2 at the first beginning */
            regval = OS_REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
            /* clear bit 25~30 main_lnaconf, alt_lnaconf, main_tb, alt_tb */
            regval &= (~(MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_LNACONF__MASK | 
                         MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_LNACONF__MASK | 
                         MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_GAINTB__MASK | 
                         MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_GAINTB__MASK)); 
            regval |= (HAL_ANT_DIV_COMB_LNA1 << 
                       MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_LNACONF__SHIFT); 
            regval |= (HAL_ANT_DIV_COMB_LNA2 << 
                       MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_LNACONF__SHIFT); 
            OS_REG_WRITE(ah, AR_PHY_MC_GAIN_CTRL, regval);
        }

    }
#endif /* ATH_ANT_DIV_COMB */
	return 0;
}


static u_int16_t ar9300AttenuationChainGet(struct ath_hal *ah, int chain, u_int16_t channel)
{
	int32_t f[3],t[3];
	u_int16_t value;
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	if (chain>=0 && chain<OSPREY_MAX_CHAINS)
	{
		if (channel<4000)
		{
			return eep->modalHeader2G.xatten1DB[chain];
		}
		else
		{
			if (eep->base_ext2.xatten1DBLow[chain]!=0)
			{		
				t[0]=eep->base_ext2.xatten1DBLow[chain];
				f[0]=5180;
				t[1]=eep->modalHeader5G.xatten1DB[chain];
				f[1]=5500;
				t[2]=eep->base_ext2.xatten1DBHigh[chain];
				f[2]=5785;
				value=interpolate(channel,f,t,3);
				return value;
			} else 
				return eep->modalHeader5G.xatten1DB[chain];
		}
	}
	return 0;
}


static u_int16_t ar9300AttenuationMarginChainGet(struct ath_hal *ah, int chain, u_int16_t channel)
{
	int32_t f[3],t[3];
	u_int16_t value;
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	if (chain>=0 && chain<OSPREY_MAX_CHAINS)
	{
		if (channel<4000)
		{
			return eep->modalHeader2G.xatten1Margin[chain];
		}
		else
		{
			if (eep->base_ext2.xatten1MarginLow[chain]!=0)
			{	
				t[0]=eep->base_ext2.xatten1MarginLow[chain];
				f[0]=5180;
				t[1]=eep->modalHeader5G.xatten1Margin[chain];
				f[1]=5500;
				t[2]=eep->base_ext2.xatten1MarginHigh[chain];
				f[2]=5785;
				value=interpolate(channel,f,t,3);
				return value;
			} else 
				return eep->modalHeader5G.xatten1Margin[chain];
		}
	}
	return 0;
}

HAL_BOOL ar9300AttenuationApply(struct ath_hal *ah, u_int16_t channel)
{
	u_int32_t value;

	//
	// Test value. if 0 then attenuation is unused. Don't load anything.
	//
	value=ar9300AttenuationChainGet(ah, 0, channel);
	OS_REG_RMW_FIELD(
        ah, AR_PHY_EXT_ATTEN_CTL_0, AR_PHY_EXT_ATTEN_CTL_XATTEN1_DB, value);
	value = ar9300AttenuationMarginChainGet(ah, 0, channel);
	OS_REG_RMW_FIELD(
        ah, AR_PHY_EXT_ATTEN_CTL_0, AR_PHY_EXT_ATTEN_CTL_XATTEN1_MARGIN, value);

	if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah))
	{
		value = ar9300AttenuationChainGet(ah, 1, channel);
		OS_REG_RMW_FIELD(
            ah, AR_PHY_EXT_ATTEN_CTL_1, AR_PHY_EXT_ATTEN_CTL_XATTEN1_DB, value);
		value = ar9300AttenuationMarginChainGet(ah, 1, channel);
		OS_REG_RMW_FIELD(
            ah, AR_PHY_EXT_ATTEN_CTL_1, AR_PHY_EXT_ATTEN_CTL_XATTEN1_MARGIN,
            value);
    	if (!AR_SREV_WASP(ah)) {
    		value = ar9300AttenuationChainGet(ah, 2, channel);
    		OS_REG_RMW_FIELD(
                ah, AR_PHY_EXT_ATTEN_CTL_2, AR_PHY_EXT_ATTEN_CTL_XATTEN1_DB, value);
	    	value = ar9300AttenuationMarginChainGet(ah, 2, channel);
	    	OS_REG_RMW_FIELD(
                ah, AR_PHY_EXT_ATTEN_CTL_2, AR_PHY_EXT_ATTEN_CTL_XATTEN1_MARGIN,
                value);
        }
	}

	return 0;
}

u_int16_t ar9300QuickDropGet(struct ath_hal *ah, int chain, u_int16_t channel)
{
	int32_t f[3],t[3];
	u_int16_t value;
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;

	if (channel < 4000) {
		return eep->modalHeader2G.quickDrop;
	} else {
		t[0]=eep->base_ext1.quickDropLow;
		f[0]=5180;
		t[1]=eep->modalHeader5G.quickDrop;
		f[1]=5500;
		t[2]=eep->base_ext1.quickDropHigh;
		f[2]=5785;
		value=interpolate(channel,f,t,3);
		return value;
	}
}


HAL_BOOL ar9300QuickDropApply(struct ath_hal *ah, u_int16_t channel)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	u_int32_t value;
	//
	// Test value. if 0 then quickDrop is unused. Don't load anything.
	//
	if (eep->baseEepHeader.miscConfiguration & 0x10)
	{
		value=ar9300QuickDropGet(ah, 0, channel);
		OS_REG_RMW_FIELD(ah, AR_PHY_AGC, AR_PHY_AGC_QUICK_DROP, value);
	}

	return 0;
}

u_int16_t ar9300txEndToXpaOffGet(struct ath_hal *ah, u_int16_t channel)
{
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;

	if (channel < 4000) {
		return eep->modalHeader2G.txEndToXpaOff;
	} else {
		return eep->modalHeader5G.txEndToXpaOff;
	}
}

HAL_BOOL ar9300TxEndToXpabOffApply(struct ath_hal *ah, u_int16_t channel)
{
 	u_int32_t value;

	value=ar9300txEndToXpaOffGet(ah, channel);
    /* Apply to both xpaa and xpab */
	OS_REG_RMW_FIELD(ah, AR_PHY_XPA_TIMING_CTL, AR_PHY_XPA_TIMING_CTL_TX_END_XPAB_OFF, value);
	OS_REG_RMW_FIELD(ah, AR_PHY_XPA_TIMING_CTL, AR_PHY_XPA_TIMING_CTL_TX_END_XPAA_OFF, value);

	return 0;
}

static int
ar9300EepromCalPierGet(struct ath_hal *ah, int mode, int ipier, int ichain, 
                       int *pfrequency, int *pcorrection, int *ptemperature, int *pvoltage)
{
    u_int8_t *pCalPier;
    OSP_CAL_DATA_PER_FREQ_OP_LOOP *pCalPierStruct;
    int is2GHz;
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;

    if (ichain >= OSPREY_MAX_CHAINS) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Invalid chain index, must be less than %d\n",
				 __func__, OSPREY_MAX_CHAINS);
        return -1;
    }

    if (mode) {//5GHz
        if (ipier >= OSPREY_NUM_5G_CAL_PIERS){
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Invalid 5GHz cal pier index, must be less than %d\n",
					 __func__, OSPREY_NUM_5G_CAL_PIERS);
            return -1;
        }
        pCalPier = &(eep->calFreqPier5G[ipier]);
        pCalPierStruct = &(eep->calPierData5G[ichain][ipier]);
        is2GHz = 0;
    } else {
        if (ipier >= OSPREY_NUM_2G_CAL_PIERS){
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Invalid 2GHz cal pier index, must be less than %d\n",
					 __func__, OSPREY_NUM_2G_CAL_PIERS);
            return -1;
        }

        pCalPier = &(eep->calFreqPier2G[ipier]);
        pCalPierStruct = &(eep->calPierData2G[ichain][ipier]);
        is2GHz = 1;
    }
    *pfrequency = FBIN2FREQ(*pCalPier, is2GHz);
    *pcorrection = pCalPierStruct->refPower;
    *ptemperature = pCalPierStruct->tempMeas;
    *pvoltage = pCalPierStruct->voltMeas;
    return 0;
}

//
// Apply the recorded correction values.
//
static int
ar9300CalibrationApply(struct ath_hal *ah, int frequency)
{
    int ichain,ipier,npier;
    int mode;
    int lfrequency[AR9300_MAX_CHAINS],lcorrection[AR9300_MAX_CHAINS],ltemperature[AR9300_MAX_CHAINS],lvoltage[AR9300_MAX_CHAINS];
    int hfrequency[AR9300_MAX_CHAINS],hcorrection[AR9300_MAX_CHAINS],htemperature[AR9300_MAX_CHAINS],hvoltage[AR9300_MAX_CHAINS];
    int fdiff;
    int correction[AR9300_MAX_CHAINS],voltage[AR9300_MAX_CHAINS],temperature[AR9300_MAX_CHAINS];
    int pfrequency,pcorrection,ptemperature,pvoltage; 
	int bf,factor,plus;

    mode=(frequency>=4000);
    if (mode)
    {
        npier=OSPREY_NUM_5G_CAL_PIERS;
    }
    else
    {
        npier=OSPREY_NUM_2G_CAL_PIERS;
    }

    for (ichain=0; ichain<AR9300_MAX_CHAINS; ichain++)
    {
        lfrequency[ichain]= 0;
        hfrequency[ichain]= 100000;
    }
    //
    // identify best lower and higher frequency calibration measurement
    //
    for (ichain=0; ichain<AR9300_MAX_CHAINS; ichain++)
    {
        for (ipier=0; ipier<npier; ipier++)
        {
            if (ar9300EepromCalPierGet(ah, mode, ipier, ichain, &pfrequency, &pcorrection, &ptemperature, &pvoltage)==0)
            {
                fdiff=frequency-pfrequency;
                //
                // this measurement is higher than our desired frequency
                //
                if (fdiff<=0)
                {
                    if (hfrequency[ichain]<=0 || hfrequency[ichain]>=100000 || fdiff>(frequency-hfrequency[ichain]))
                    {
                        //
                        // new best higher frequency measurement
                        //
                        hfrequency[ichain]=pfrequency;
                        hcorrection[ichain]=pcorrection;
                        htemperature[ichain]=ptemperature;
                        hvoltage[ichain]=pvoltage;
                    }
                }
                if (fdiff>=0)
                {
                    if (lfrequency[ichain]<=0 || fdiff<(frequency-lfrequency[ichain]))
                    {
                        //
                        // new best lower frequency measurement
                        //
                        lfrequency[ichain]=pfrequency;
                        lcorrection[ichain]=pcorrection;
                        ltemperature[ichain]=ptemperature;
                        lvoltage[ichain]=pvoltage;
                    }
                }
            }
        }
    }
    //
    // interpolate 
    //
    for (ichain=0; ichain<AR9300_MAX_CHAINS; ichain++)
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: ch=%d f=%d low=%d %d h=%d %d\n",
				 __func__, ichain, frequency, lfrequency[ichain], lcorrection[ichain],
				 hfrequency[ichain], hcorrection[ichain]);
        //
        // they're the same, so just pick one
        //
        if (hfrequency[ichain]==lfrequency[ichain])
        {
            correction[ichain]=lcorrection[ichain];
            voltage[ichain]=lvoltage[ichain];
            temperature[ichain]=ltemperature[ichain];
        }
        //
        // the low frequency is good
        //
        else if (frequency-lfrequency[ichain]<1000)
        {
            //
            // so is the high frequency, interpolate with round off
            //
            if (hfrequency[ichain]-frequency<1000)
            {
                bf=(2*(hcorrection[ichain]-lcorrection[ichain])*(frequency-lfrequency[ichain]))/(hfrequency[ichain]-lfrequency[ichain]);
				plus=(bf%2);
				factor=bf/2;
                correction[ichain]=lcorrection[ichain]+factor+plus;

				bf=(2*(htemperature[ichain]-ltemperature[ichain])*(frequency-lfrequency[ichain]))/(hfrequency[ichain]-lfrequency[ichain]);
				plus=(bf%2);
				factor=bf/2;
                temperature[ichain]=ltemperature[ichain]+factor+plus;

				bf=(2*(hvoltage[ichain]-lvoltage[ichain])*(frequency-lfrequency[ichain]))/(hfrequency[ichain]-lfrequency[ichain]);
				plus=(bf%2);
				factor=bf/2;
                voltage[ichain]=lvoltage[ichain]+factor+plus;
            }
            //
            // only low is good, use it
            //
            else
            {
                correction[ichain]=lcorrection[ichain];
                temperature[ichain]=ltemperature[ichain];
                voltage[ichain]=lvoltage[ichain];
            }
        }
        //
        // only high is good, use it
        //
        else if (hfrequency[ichain]-frequency<1000)
        {
            correction[ichain]=hcorrection[ichain];
            temperature[ichain]=htemperature[ichain];
            voltage[ichain]=hvoltage[ichain];
        }
        //
        // nothing is good, presume 0????
        //
        else
        {
            correction[ichain]=0;
            temperature[ichain]=0;
            voltage[ichain]=0;
        }
    }

    /* GreenTx */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable) {
        if (AR_SREV_POSEIDON(ah)) {
            /* Get calibrated OLPC gain delta value for GreenTx */
            ah->DB2[POSEIDON_STORED_REG_G2_OLPC_OFFSET] = 
                (u_int32_t) correction[0];
        }
    }

    ar9300PowerControlOverride(ah, frequency, correction,voltage,temperature);
    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: for frequency=%d, calibration correction = %d %d %d\n",
			 __func__, frequency, correction[0], correction[1], correction[2]);

    return 0;
}

int
ar9300PowerControlOverride(struct ath_hal *ah, int frequency, int *correction, int *voltage, int *temperature)
{
    int tempSlope = 0;
    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	int32_t f[3],t[3];

    OS_REG_RMW(ah, AR_PHY_TPC_11_B0,
    (correction[0] << AR_PHY_TPC_OLPC_GAIN_DELTA_S), AR_PHY_TPC_OLPC_GAIN_DELTA);
    if (!AR_SREV_POSEIDON(ah)) {
        OS_REG_RMW(ah, AR_PHY_TPC_11_B1,
            (correction[1] << AR_PHY_TPC_OLPC_GAIN_DELTA_S),  AR_PHY_TPC_OLPC_GAIN_DELTA);
        if(!AR_SREV_WASP(ah)) {
            OS_REG_RMW(ah, AR_PHY_TPC_11_B2, 
                (correction[2] << AR_PHY_TPC_OLPC_GAIN_DELTA_S), AR_PHY_TPC_OLPC_GAIN_DELTA);
        }
    }
    //
    // enable open loop power control on chip
    //
    OS_REG_RMW(ah, AR_PHY_TPC_6_B0,
    (3 << AR_PHY_TPC_6_ERROR_EST_MODE_S), AR_PHY_TPC_6_ERROR_EST_MODE);
    if (!AR_SREV_POSEIDON(ah)) {
        OS_REG_RMW(ah, AR_PHY_TPC_6_B1, 
            (3 << AR_PHY_TPC_6_ERROR_EST_MODE_S), AR_PHY_TPC_6_ERROR_EST_MODE);
        if(!AR_SREV_WASP(ah)) {
            OS_REG_RMW(ah, AR_PHY_TPC_6_B2, 
                (3 << AR_PHY_TPC_6_ERROR_EST_MODE_S), AR_PHY_TPC_6_ERROR_EST_MODE);
        }
    }

    //
    //enable temperature compensation
    //Need to use register names
    if (frequency < 4000) {
        tempSlope = eep->modalHeader2G.tempSlope;
    }
    else {
		if (eep->base_ext2.tempSlopeLow!=0)
		{
			t[0]=eep->base_ext2.tempSlopeLow;
			f[0]=5180;
			t[1]=eep->modalHeader5G.tempSlope;
			f[1]=5500;
			t[2]=eep->base_ext2.tempSlopeHigh;
			f[2]=5785;
			tempSlope=interpolate(frequency,f,t,3);
		}
		else
		{
			tempSlope = eep->modalHeader5G.tempSlope;
		}
    }
    
    OS_REG_RMW_FIELD(ah, AR_PHY_TPC_19, AR_PHY_TPC_19_ALPHA_THERM, tempSlope);
    OS_REG_RMW_FIELD(ah, AR_PHY_TPC_18, AR_PHY_TPC_18_THERM_CAL_VALUE, temperature[0]);

    return 0;
}

/**************************************************************
 * ar9300EepDefGetMaxEdgePower
 *
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static inline u_int16_t
ar9300EepDefGetMaxEdgePower(ar9300_eeprom_t *pEepData, u_int16_t freq, int idx, HAL_BOOL is2GHz)
{
    u_int16_t twiceMaxEdgePower = AR9300_MAX_RATE_POWER;
    u_int8_t *ctl_freqbin = is2GHz ? &pEepData->ctl_freqbin_2G[idx][0] : &pEepData->ctl_freqbin_5G[idx][0];
    u_int16_t num_edges = is2GHz ? OSPREY_NUM_BAND_EDGES_2G : OSPREY_NUM_BAND_EDGES_5G;
    int i;

    /* Get the edge power */
    for (i = 0; (i < num_edges) && (ctl_freqbin[i] != AR9300_BCHAN_UNUSED) ; i++)
    {
        /*
        * If there's an exact channel match or an inband flag set
        * on the lower channel use the given rdEdgePower
 */

        if (freq == fbin2freq(ctl_freqbin[i], is2GHz))
        {
            if (is2GHz) {
				// The following if-else section which overrides CTL power limits
				// is due to wrong values in the CTL table in the EEPROM template.
				// Please refer to EV89475 for more details.
				// Check the CTL value in the driver, and override with 30dBm if
				// Template version is 2 and a Tx power value of zero is seen.
				// Note that Tx power values are not listed in units of 1dBm in
				// the struct, but rather in 0.5 dBm units. Hence, a value of
				// 60 actually indicates 30 dBm
                if ((2 == pEepData->templateVersion) &&
					(0 == pEepData->ctlPowerData_2G[idx].ctlEdges[i].tPower)) {
					twiceMaxEdgePower = 60;
				} else {
					twiceMaxEdgePower = pEepData->ctlPowerData_2G[idx].ctlEdges[i].tPower;
				}
            } else {       
                twiceMaxEdgePower = pEepData->ctlPowerData_5G[idx].ctlEdges[i].tPower;
            }
            break;
        }
        else if ((i > 0) && (freq < fbin2freq(ctl_freqbin[i], is2GHz)))
        {
            if (is2GHz) {
                if (fbin2freq(ctl_freqbin[i - 1], 1) < freq && pEepData->ctlPowerData_2G[idx].ctlEdges[i - 1].flag)
                {
					// The following if-else section which overrides CTL power
					// limits is due to wrong values in the CTL table in the
					// EEPROM template. Please refer to EV89475 for more details.
					// Check the CTL value in the driver, and override with
					// 30dBm if Template version is 2 and a Tx power value
					// of zero is seen.
					// Note that Tx power values are not listed in units of
					// 1dBm in the struct, but rather in 0.5 dBm units. Hence,
					// a value of 60 actually indicates 30 dBm
					if ((2 == pEepData->templateVersion) &&
						(0 == pEepData->ctlPowerData_2G[idx].ctlEdges[i - 1].tPower)) {
						twiceMaxEdgePower = 60;
					} else {
						twiceMaxEdgePower = pEepData->ctlPowerData_2G[idx].ctlEdges[i - 1].tPower;
					}
               }
            } else {
                if (fbin2freq(ctl_freqbin[i - 1], 0) < freq && pEepData->ctlPowerData_5G[idx].ctlEdges[i - 1].flag)
                {
                    twiceMaxEdgePower = pEepData->ctlPowerData_5G[idx].ctlEdges[i - 1].tPower;
                }
            }
            /* Leave loop - no more affecting edges possible in this monotonic increasing list */
            break;
        }
    }
    HALASSERT(twiceMaxEdgePower > 0);
    return twiceMaxEdgePower;
}


HAL_BOOL
ar9300EepromSetPowerPerRateTable(struct ath_hal *ah, ar9300_eeprom_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan,
    u_int8_t *pPwrArray, u_int16_t cfgCtl,
    u_int16_t AntennaReduction,
    u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit, u_int8_t chainmask)
{
    /* Local defines to distinguish between extension and control CTL's */
#define EXT_ADDITIVE (0x8000)
#define CTL_11A_EXT (CTL_11A | EXT_ADDITIVE)
#define CTL_11G_EXT (CTL_11G | EXT_ADDITIVE)
#define CTL_11B_EXT (CTL_11B | EXT_ADDITIVE)
#define REDUCE_SCALED_POWER_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define REDUCE_SCALED_POWER_BY_THREE_CHAIN  10  /* 10*log10(3)*2 */
#define PWRINCR_3_TO_1_CHAIN      9             /* 10*log(3)*2 */
#define PWRINCR_3_TO_2_CHAIN      3             /* floor(10*log(3/2)*2) */
#define PWRINCR_2_TO_1_CHAIN      6             /* 10*log(2)*2 */

    u_int16_t twiceMaxEdgePower = AR9300_MAX_RATE_POWER;
    static const u_int16_t tpScaleReductionTable[5] = { 0, 3, 6, 9, AR9300_MAX_RATE_POWER };

    int i;
    int16_t  twiceLargestAntenna;
    u_int16_t twiceAntennaReduction = 2*AntennaReduction ;
    int16_t scaledPower=0, minCtlPower, maxRegAllowedPower;
#define SUB_NUM_CTL_MODES_AT_5G_40 2    /* excluding HT40, EXT-OFDM */
#define SUB_NUM_CTL_MODES_AT_2G_40 3    /* excluding HT40, EXT-OFDM, EXT-CCK */
    u_int16_t ctlModesFor11a[] = {CTL_11A, CTL_5GHT20, CTL_11A_EXT, CTL_5GHT40};
    u_int16_t ctlModesFor11g[] = {CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT, CTL_11G_EXT, CTL_2GHT40};
    u_int16_t numCtlModes, *pCtlMode, ctlMode, freq;
    CHAN_CENTERS centers;
    int tx_chainmask;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int8_t *ctlIndex;
    u_int8_t ctlNum;
    u_int16_t twiceMinEdgePower;

    tx_chainmask = chainmask ? chainmask : ahp->ah_txchainmask;

    ar9300GetChannelCenters(ah, chan, &centers);

    /* Get Antenna Gain from EEPROM */
    if (IS_CHAN_2GHZ(chan)) {
        ahp->twiceAntennaGain = pEepData->modalHeader2G.antennaGain;
    } else {
        ahp->twiceAntennaGain = pEepData->modalHeader5G.antennaGain;
    }

    /* Save max allowed antenna gain to ease future lookups */
    ahp->twiceAntennaReduction = twiceAntennaReduction; 

    /*  Deduct antenna gain from  EIRP to get the upper limit */
    twiceLargestAntenna = (int16_t)AH_MIN((twiceAntennaReduction - ahp->twiceAntennaGain), 0);
    maxRegAllowedPower = twiceMaxRegulatoryPower + twiceLargestAntenna;

    /* Use ah_tpScale - see bug 30070.*/
    if (AH_PRIVATE(ah)->ah_tpScale != HAL_TP_SCALE_MAX) { 
        maxRegAllowedPower -= (tpScaleReductionTable[(AH_PRIVATE(ah)->ah_tpScale)] * 2);
    }

    scaledPower = AH_MIN(powerLimit, maxRegAllowedPower);


    /* Reduce scaled Power by number of chains active to get to per chain tx power level */
    switch (ar9300_get_ntxchains(tx_chainmask))
    {
    case 1:
        ahp->upperLimit[0] = AH_MAX(0, scaledPower);
        break;
    case 2:
        scaledPower -= REDUCE_SCALED_POWER_BY_TWO_CHAIN;
        ahp->upperLimit[1] = AH_MAX(0, scaledPower);
        break;
    case 3:
        scaledPower -= REDUCE_SCALED_POWER_BY_THREE_CHAIN;
        ahp->upperLimit[2] = AH_MAX(0, scaledPower);
        break;
    default:
        HALASSERT(0); /* Unsupported number of chains */
    }

    scaledPower = AH_MAX(0, scaledPower);

    /*
    * Get target powers from EEPROM - our baseline for TX Power
    */
    if (IS_CHAN_2GHZ(chan))
    {
        /* Setup for CTL modes */
        numCtlModes = N(ctlModesFor11g) - SUB_NUM_CTL_MODES_AT_2G_40; /* CTL_11B, CTL_11G, CTL_2GHT20 */
        pCtlMode = ctlModesFor11g;

        if (IS_CHAN_HT40(chan))
    {
            numCtlModes = N(ctlModesFor11g);    /* All 2G CTL's */
        }
    }
    else
    {
        /* Setup for CTL modes */
        numCtlModes = N(ctlModesFor11a) - SUB_NUM_CTL_MODES_AT_5G_40; /* CTL_11A, CTL_5GHT20 */
        pCtlMode = ctlModesFor11a;

        if (IS_CHAN_HT40(chan))
    {
            numCtlModes = N(ctlModesFor11a); /* All 5G CTL's */
        }
    }

    /*
    * For MIMO, need to apply regulatory caps individually across dynamically
    * running modes: CCK, OFDM, HT20, HT40
    *
    * The outer loop walks through each possible applicable runtime mode.
    * The inner loop walks through each ctlIndex entry in EEPROM.
    * The ctl value is encoded as [7:4] == test group, [3:0] == test mode.
    *
    */
    for (ctlMode = 0; ctlMode < numCtlModes; ctlMode++)
    {

        HAL_BOOL isHt40CtlMode = (pCtlMode[ctlMode] == CTL_5GHT40) || (pCtlMode[ctlMode] == CTL_2GHT40);
        if (isHt40CtlMode)
        {
            freq = centers.synth_center;
        }
        else if (pCtlMode[ctlMode] & EXT_ADDITIVE)
        {
            freq = centers.ext_center;
        }
        else
        {
            freq = centers.ctl_center;
        }

        HDPRINTF(ah, HAL_DBG_POWER_MGMT, "LOOP-Mode ctlMode %d < %d, isHt40CtlMode %d, EXT_ADDITIVE %d\n",
            ctlMode, numCtlModes, isHt40CtlMode, (pCtlMode[ctlMode] & EXT_ADDITIVE));

        /* Walk through each CTL index stored in EEPROM */
        if (IS_CHAN_2GHZ(chan)) {
            ctlIndex = pEepData->ctlIndex_2G;
            ctlNum = OSPREY_NUM_CTLS_2G;
        } else {
            ctlIndex = pEepData->ctlIndex_5G;
            ctlNum = OSPREY_NUM_CTLS_5G;
        }

        for (i = 0; (i < ctlNum) && ctlIndex[i]; i++)
        {
            HDPRINTF(ah, HAL_DBG_POWER_MGMT, 
                "  LOOP-Ctlidx %d: cfgCtl 0x%2.2x pCtlMode 0x%2.2x ctlIndex 0x%2.2x chan %d chanctl 0x%x\n",
                i, cfgCtl, pCtlMode[ctlMode], ctlIndex[i], 
                chan->channel, chan->conformanceTestLimit);

            /* compare test group from regulatory channel list with test mode from pCtlMode list */
            if ((((cfgCtl & ~CTL_MODE_M) | (pCtlMode[ctlMode] & CTL_MODE_M)) == ctlIndex[i]) ||
                (((cfgCtl & ~CTL_MODE_M) | (pCtlMode[ctlMode] & CTL_MODE_M)) == ((ctlIndex[i] & CTL_MODE_M) | SD_NO_CTL)))
            {
                twiceMinEdgePower = ar9300EepDefGetMaxEdgePower(pEepData, freq, i, IS_CHAN_2GHZ(chan));

                HDPRINTF(ah, HAL_DBG_POWER_MGMT,
                         "    MATCH-EE_IDX %d: ch %d is2 %d 2xMinEdge %d chainmask %d chains %d\n",
                         i, freq, IS_CHAN_2GHZ(chan), twiceMinEdgePower, tx_chainmask,
                         ar9300_get_ntxchains(tx_chainmask));

                if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL)
                {
                    /* Find the minimum of all CTL edge powers that apply to this channel */
                    twiceMaxEdgePower = AH_MIN(twiceMaxEdgePower, twiceMinEdgePower);
                }
                else
                {
                    /* specific */
                    twiceMaxEdgePower = twiceMinEdgePower;
                    break;
                }
            }
        }

        minCtlPower = (u_int8_t)AH_MIN(twiceMaxEdgePower, scaledPower);

        HDPRINTF(ah, HAL_DBG_POWER_MGMT,
                 "    SEL-Min ctlMode %d pCtlMode %d 2xMaxEdge %d sP %d minCtlPwr %d\n",
                 ctlMode, pCtlMode[ctlMode], twiceMaxEdgePower, scaledPower, minCtlPower);


        /* Apply ctl mode to correct target power set */
        switch(pCtlMode[ctlMode])
        {
        case CTL_11B:
            for (i = ALL_TARGET_LEGACY_1L_5L; i <= ALL_TARGET_LEGACY_11S; i++)
            {
                pPwrArray[i] = (u_int8_t)AH_MIN(pPwrArray[i], minCtlPower);
            }
            break;
        case CTL_11A:
        case CTL_11G:
            for (i = ALL_TARGET_LEGACY_6_24; i <= ALL_TARGET_LEGACY_54; i++)
            {
                pPwrArray[i] = (u_int8_t)AH_MIN(pPwrArray[i], minCtlPower);
#ifdef ATH_BT_COEX
                if ((ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) &&
                    (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) && 
                    (ahp->ah_btWlanIsolation < HAL_BT_COEX_ISOLATION_FOR_NO_COEX))
                {
                    u_int8_t reducePow = (HAL_BT_COEX_ISOLATION_FOR_NO_COEX - ahp->ah_btWlanIsolation) << 1;

                    if (reducePow <= pPwrArray[i]) {
                        pPwrArray[i] -= reducePow;
                    }
                }
#endif
            }
            break;
        case CTL_5GHT20:
        case CTL_2GHT20:
            for (i = ALL_TARGET_HT20_0_8_16; i <= ALL_TARGET_HT20_23; i++)
            {
                pPwrArray[i] = (u_int8_t)AH_MIN(pPwrArray[i], minCtlPower);
#ifdef ATH_BT_COEX
                if ((ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) &&
                    (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) && 
                    (ahp->ah_btWlanIsolation < HAL_BT_COEX_ISOLATION_FOR_NO_COEX))
                {
                    u_int8_t reducePow = (HAL_BT_COEX_ISOLATION_FOR_NO_COEX - ahp->ah_btWlanIsolation) << 1;

                    if (reducePow <= pPwrArray[i]) {
                        pPwrArray[i] -= reducePow;
                    }
                }
#endif
            }
            break;
#ifdef NOT_YET
        case CTL_11B_EXT:
            targetPowerCckExt.tPow2x[0] = (u_int8_t)AH_MIN(targetPowerCckExt.tPow2x[0], minCtlPower);
            break;
        case CTL_11A_EXT:
        case CTL_11G_EXT:
            targetPowerOfdmExt.tPow2x[0] = (u_int8_t)AH_MIN(targetPowerOfdmExt.tPow2x[0], minCtlPower);
            break;
#endif /* NOT_YET */
        case CTL_5GHT40:
        case CTL_2GHT40:
            for (i = ALL_TARGET_HT40_0_8_16; i <= ALL_TARGET_HT40_23; i++)
            {
                pPwrArray[i] = (u_int8_t)AH_MIN(pPwrArray[i], minCtlPower);
#ifdef ATH_BT_COEX
                if ((ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) &&
                    (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) && 
                    (ahp->ah_btWlanIsolation < HAL_BT_COEX_ISOLATION_FOR_NO_COEX))
                {
                    u_int8_t reducePow = (HAL_BT_COEX_ISOLATION_FOR_NO_COEX - ahp->ah_btWlanIsolation) << 1;

                    if (reducePow <= pPwrArray[i]) {
                        pPwrArray[i] -= reducePow;
                    }
                }
#endif
            }
            break;
        default:
            HALASSERT(0);
            break;
        }
    } /* end ctl mode checking */

    return AH_TRUE;
#undef EXT_ADDITIVE
#undef CTL_11A_EXT
#undef CTL_11G_EXT
#undef CTL_11B_EXT
#undef REDUCE_SCALED_POWER_BY_TWO_CHAIN
#undef REDUCE_SCALED_POWER_BY_THREE_CHAIN
    }

/**************************************************************
 * ar9300EepromSetTransmitPower
 *
 * Set the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_STATUS
ar9300EepromSetTransmitPower(struct ath_hal *ah,
    ar9300_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t antennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
#define ABS(_x, _y) ((int)_x > (int)_y ? (int)_x - (int)_y : (int)_y - (int)_x)
#define INCREASE_MAXPOW_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define INCREASE_MAXPOW_BY_THREE_CHAIN  10 /* 10*log10(3)*2 */

    u_int8_t targetPowerValT2[ar9300RateSize];
    u_int8_t targetPowerValT2EEP[ar9300RateSize];
    int16_t twiceArrayGain = 0, maxPowerLevel = 0;
    struct ath_hal_9300 *ahp = AH9300(ah);
    int  i = 0;
    u_int32_t tmpPapdRateMask = 0, *tmpPtr = NULL;
    int      paprdScaleFactor = 5;
	u_int8_t *ptrMcsRate2powerTableIndex;
	u_int8_t mcsRate2powerTableIndexht20[24] =
	{
		ALL_TARGET_HT20_0_8_16,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_4,
		ALL_TARGET_HT20_5,
		ALL_TARGET_HT20_6,
		ALL_TARGET_HT20_7,
		ALL_TARGET_HT20_0_8_16,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_12,
		ALL_TARGET_HT20_13,
		ALL_TARGET_HT20_14,
		ALL_TARGET_HT20_15,
		ALL_TARGET_HT20_0_8_16,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_1_3_9_11_17_19,
		ALL_TARGET_HT20_1_3_9_11_17_19,
	    ALL_TARGET_HT20_20,
	    ALL_TARGET_HT20_21,
	    ALL_TARGET_HT20_22,
	    ALL_TARGET_HT20_23
	};

	u_int8_t mcsRate2powerTableIndexht40[24]=
	{
		ALL_TARGET_HT40_0_8_16,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_4,
	    ALL_TARGET_HT40_5,
	    ALL_TARGET_HT40_6,
	    ALL_TARGET_HT40_7,
		ALL_TARGET_HT40_0_8_16,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_12,
	    ALL_TARGET_HT40_13,
	    ALL_TARGET_HT40_14,
	    ALL_TARGET_HT40_15,
		ALL_TARGET_HT40_0_8_16,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_1_3_9_11_17_19,
	    ALL_TARGET_HT40_20,
		ALL_TARGET_HT40_21,
		ALL_TARGET_HT40_22,
		ALL_TARGET_HT40_23,
	};

    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] +++chan %d,cfgctl 0x%04x  antennaReduction 0x%04x, twiceMaxRegulatoryPower 0x%04x powerLimit 0x%04x\n", __func__, __LINE__, 
                        chan->channel, cfgCtl, antennaReduction, twiceMaxRegulatoryPower, powerLimit);
    ar9300SetTargetPowerFromEeprom(ah, chan->channel, targetPowerValT2);

    if (ar9300EepromGet(ahp, EEP_PAPRD_ENABLED)) {
        if (IS_CHAN_2GHZ(chan))
        {
            if(IS_CHAN_HT40(chan))
            {
                tmpPapdRateMask = pEepData->modalHeader2G.papdRateMaskHt40;
                tmpPtr = &AH9300(ah)->ah_2G_papdRateMaskHt40;
            } else {
                tmpPapdRateMask = pEepData->modalHeader2G.papdRateMaskHt20;
                tmpPtr = &AH9300(ah)->ah_2G_papdRateMaskHt20;
            }
        }
        else
        {
            if (IS_CHAN_HT40(chan))
            {
                tmpPapdRateMask = pEepData->modalHeader5G.papdRateMaskHt40;
                tmpPtr = &AH9300(ah)->ah_5G_papdRateMaskHt40;
            } else {
                tmpPapdRateMask = pEepData->modalHeader5G.papdRateMaskHt20;
                tmpPtr = &AH9300(ah)->ah_5G_papdRateMaskHt20;
            }
        }
        AH_PAPRD_GET_SCALE_FACTOR(paprdScaleFactor, pEepData, IS_CHAN_2GHZ(chan), chan->channel);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] paprdScaleFactor %d\n", __func__, __LINE__, paprdScaleFactor);

        /* PAPRD is not done yet, Scale down the EEP power */
        if(IS_CHAN_HT40(chan))
		{
			ptrMcsRate2powerTableIndex = &mcsRate2powerTableIndexht40[0];
		} else {
			ptrMcsRate2powerTableIndex = &mcsRate2powerTableIndexht20[0];
		}
		if (!chan->paprdTableWriteDone) {
            for (i = 0;  i < 24; i++)
            {
                /* PAPRD is done yet, so Scale down Power for PAPRD Rates*/
                if (tmpPapdRateMask & (1 << i)) {
                    targetPowerValT2[ptrMcsRate2powerTableIndex[i]] -=  paprdScaleFactor;
                    HDPRINTF(ah, HAL_DBG_CALIBRATE,
                         "%s[%d]: Chan %d Scale down targetPowerValT2[%d] = 0x%04x\n",
                         __func__, __LINE__, chan->channel, i, targetPowerValT2[i]);
                }
            }
        }
        else {
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d]: PAPRD Done No TGT PWR Scaling\n", __func__, __LINE__);
        }
    }

    /* Save the Target power for future use */
    OS_MEMCPY(targetPowerValT2EEP, targetPowerValT2, sizeof(targetPowerValT2));
    ar9300EepromSetPowerPerRateTable(ah, pEepData, chan,
                                     targetPowerValT2, cfgCtl,
                                     antennaReduction,
                                     twiceMaxRegulatoryPower,
                                     powerLimit, 0);
    
    /* Save this for quick lookup */
    ahp->regDmn = chan->conformanceTestLimit;

    /*
     * Always use CDD/direct per rate power table for register based approach.
     * For FCC, CDD calculations should factor in the array gain, hence 
     * this adjust call. ETSI and MKK does not have this requirement.
     */
    if (isRegDmnFCC(ahp->regDmn)) {
        ar9300AdjustRegTxPowerCDD(ah, targetPowerValT2);
    }

#ifndef ART_BUILD
    if (ar9300EepromGet(ahp, EEP_PAPRD_ENABLED)) {
        for (i = 0;  i < ar9300RateSize; i++)
        {
            /* EEPROM TGT PWR is not same as current TGT PWR, so Disable PAPRD for this rate.
                      *  Some of APs might ask to reduce Target Power, if target power drops significantly,
                      *  disable PAPRD for that rate.
                      */
            if (tmpPapdRateMask & (1 << i)) {
                if (ABS(targetPowerValT2EEP[i], targetPowerValT2[i]) > paprdScaleFactor) 
                {
                    tmpPapdRateMask &= ~(1 << i);
                    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: EEP TPC[%02d] 0x%08x Curr TPC[%02d] 0x%08x mask = 0x%08x\n",
    					 __func__, i, targetPowerValT2EEP[i], i, targetPowerValT2[i], tmpPapdRateMask);
                }
            }

        }
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Chan %d After tmpPapdRateMask = 0x%08x\n", __func__, chan->channel, tmpPapdRateMask);
        if(tmpPtr)
            *tmpPtr = tmpPapdRateMask;
    }
#endif

    // Write target power array to registers
    ar9300TransmitPowerRegWrite(ah, targetPowerValT2);

    /* Write target power for self generated frames to the TPC register */
    ar9300_selfgen_tpc_reg_write(ah, chan, targetPowerValT2);

    /* GreenTx or Paprd */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable || 
        AH_PRIVATE(ah)->ah_caps.halPaprdEnabled) 
    {
        if (AR_SREV_POSEIDON(ah)) {
            /*For HAL_RSSI_TX_POWER_NONE array*/
            OS_MEMCPY(ahp->ah_default_tx_power, 
                targetPowerValT2, 
                sizeof(targetPowerValT2));
            /* Get defautl tx related register setting for GreenTx */
            /* Record OB/DB */
            ah->OBDB1[POSEIDON_STORED_REG_OBDB] = 
                OS_REG_READ(ah, AR_PHY_65NM_CH0_TXRF2);
            /* Record TPC settting */
            ah->OBDB1[POSEIDON_STORED_REG_TPC]= OS_REG_READ(ah, AR_TPC);
            /* Record BB_powertx_rate9 setting */ 
            ah->OBDB1[POSEIDON_STORED_REG_BB_PWRTX_RATE9] = 
                OS_REG_READ(ah, AR_PHY_BB_POWERTX_RATE9);
        }
    }
    /*
    * Return tx power used to iwconfig.
    * Since power is rate dependent, use one of the indices from the
    * AR9300_Rates enum to select an entry from targetPowerValT2[] to report.
    * Currently returns the power for HT40 MCS 0, HT20 MCS 0, or OFDM 6 Mbps
    * as CCK power is less interesting (?).
    */
    i = ALL_TARGET_LEGACY_6_24;         /* legacy */
    if (IS_CHAN_HT40(chan))
    {
        i = ALL_TARGET_HT40_0_8_16;     /* ht40 */
    }
    else if (IS_CHAN_HT20(chan))
    {
        i = ALL_TARGET_HT20_0_8_16;     /* ht20 */
    }

    maxPowerLevel = targetPowerValT2[i];
    /* Adjusting the ah_maxPowerLevel based on chains and antennaGain*/
    switch (ar9300_get_ntxchains(ahp->ah_txchainmask))
    {
        case 1:
            break;
        case 2:
            twiceArrayGain = (ahp->twiceAntennaGain >= ahp->twiceAntennaReduction)? 0:
                               ((int16_t)AH_MIN((ahp->twiceAntennaReduction -
                                   (ahp->twiceAntennaGain + INCREASE_MAXPOW_BY_TWO_CHAIN)), 0));
            /* Adjusting maxpower with antennaGain */
            maxPowerLevel -= twiceArrayGain;
            /* Adjusting maxpower based on chain */
            maxPowerLevel += INCREASE_MAXPOW_BY_TWO_CHAIN;
            break;
        case 3:
            twiceArrayGain = (ahp->twiceAntennaGain >= ahp->twiceAntennaReduction)? 0:
                               ((int16_t)AH_MIN((ahp->twiceAntennaReduction -
                                   (ahp->twiceAntennaGain + INCREASE_MAXPOW_BY_THREE_CHAIN)), 0));

            /* Adjusting maxpower with antennaGain */
            maxPowerLevel -= twiceArrayGain;
            /* Adjusting maxpower based on chain */
            maxPowerLevel += INCREASE_MAXPOW_BY_THREE_CHAIN;
            break;
        default:
            HALASSERT(0); /* Unsupported number of chains */
    }
    AH_PRIVATE(ah)->ah_maxPowerLevel = (int8_t)maxPowerLevel;

    ar9300CalibrationApply(ah, chan->channel);
#undef ABS

    /* Handle per packet TPC initializations */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc) {
        /* Transmit Power per-rate per-chain  are  computed here. A separate
         * power table is maintained for different MIMO modes (i.e. TXBF ON,
         * STBC) to enable easy lookup during packet transmit. 
         * The reason for maintaing each of these tables per chain is that
         * the transmit power used for different number of chains is different
         * depending on whether the power has been limited by the target power,
         * the regulatory domain  or the CTL limits.
         */
        u_int mode = ath_hal_get_curmode(ah, chan);
        u_int32_t val = 0;
        u_int8_t chainmasks[AR9300_MAX_CHAINS] =
            {OSPREY_1_CHAINMASK, OSPREY_2LOHI_CHAINMASK, OSPREY_3_CHAINMASK};
        for (i = 0; i < AR9300_MAX_CHAINS; i++) {
            OS_MEMCPY(targetPowerValT2, targetPowerValT2EEP, sizeof(targetPowerValT2EEP));
            ar9300EepromSetPowerPerRateTable(ah, pEepData, chan,
                                     targetPowerValT2, cfgCtl,
                                     antennaReduction,
                                     twiceMaxRegulatoryPower,
                                     powerLimit, chainmasks[i]);
            HDPRINTF(ah, HAL_DBG_POWER_MGMT,
                         " Channel = %d Chainmask = %d, Upper Limit = [%2d.%1d dBm]\n",
                            chan->channel,i, ahp->upperLimit[i]/2, ahp->upperLimit[i]%2 * 5);
            ar9300InitRateTxPower(ah, mode, chan, targetPowerValT2, chainmasks[i]);
                                     
        }

        /* Enable TPC */
        OS_REG_WRITE(ah, AR_PHY_PWRTX_MAX, AR_PHY_PWRTX_MAX_TPC_ENABLE);
        /*
         * Disable per chain power reduction since we are already 
         * accounting for this in our calculations 
         */
        val = OS_REG_READ(ah, AR_PHY_POWER_TX_SUB);
        if (AR_SREV_WASP(ah)) {
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_SUB, val & AR_PHY_POWER_TX_SUB_2_DISABLE);
        } else {
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_SUB, val & AR_PHY_POWER_TX_SUB_3_DISABLE);
        }
    }

    return HAL_OK;
}

/**************************************************************
 * ar9300EepromSetAddac
 *
 * Set the ADDAC from eeprom.
 */
void
ar9300EepromSetAddac(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{

    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                 "FIXME: ar9300EepromDefSetAddac called\n");
#if 0
    MODAL_EEPDEF_HEADER *pModal;
    struct ath_hal_9300 *ahp = AH9300(ah);
    ar9300_eeprom_t *eep = &ahp->ah_eeprom.def;
    u_int8_t biaslevel;

    if (AH_PRIVATE(ah)->ah_macVersion != AR_SREV_VERSION_SOWL)
        return;

    HALASSERT(owl_get_eepdef_ver(ahp) == AR9300_EEP_VER);

    /* Xpa bias levels in eeprom are valid from rev 14.7 */
    if (owl_get_eepdef_rev(ahp) < AR9300_EEP_MINOR_VER_7)
        return;

    if (ahp->ah_emu_eeprom)
        return;

    pModal = &(eep->modalHeader[IS_CHAN_2GHZ(chan)]);

    if (pModal->xpaBiasLvl != 0xff) {
        biaslevel = pModal->xpaBiasLvl;
    } else {
        /* Use freqeuncy specific xpa bias level */
        u_int16_t resetFreqBin, freqBin, freqCount=0;
        CHAN_CENTERS centers;

        ar9300GetChannelCenters(ah, chan, &centers);

        resetFreqBin = FREQ2FBIN(centers.synth_center, IS_CHAN_2GHZ(chan));
        freqBin = pModal->xpaBiasLvlFreq[0] & 0xff;
        biaslevel = (u_int8_t)(pModal->xpaBiasLvlFreq[0] >> 14);

        freqCount++;

        while (freqCount < 3) {
            if (pModal->xpaBiasLvlFreq[freqCount] == 0x0)
                break;

            freqBin = pModal->xpaBiasLvlFreq[freqCount] & 0xff;
            if (resetFreqBin >= freqBin) {
                biaslevel = (u_int8_t)(pModal->xpaBiasLvlFreq[freqCount] >> 14);
            } else {
                break;
            }
            freqCount++;
        }
    }

    /* Apply bias level to the ADDAC values in the INI array */
    if (IS_CHAN_2GHZ(chan)) {
        INI_RA(&ahp->ah_iniAddac, 7, 1) =
            (INI_RA(&ahp->ah_iniAddac, 7, 1) & (~0x18)) | biaslevel << 3;
    } else {
        INI_RA(&ahp->ah_iniAddac, 6, 1) =
            (INI_RA(&ahp->ah_iniAddac, 6, 1) & (~0xc0)) | biaslevel << 6;
    }
#endif
}

u_int
ar9300EepromDumpSupport(struct ath_hal *ah, void **ppE)
{
    *ppE = &(AH9300(ah)->ah_eeprom);
    return sizeof(ar9300_eeprom_t);
}

u_int8_t
ar9300EepromGetNumAntConfig(struct ath_hal_9300 *ahp, HAL_FREQ_BAND freq_band)
{
# if 0
    ar9300_eeprom_t  *eep = &ahp->ah_eeprom.def;
    MODAL_EEPDEF_HEADER *pModal = &(eep->modalHeader[HAL_FREQ_BAND_2GHZ == freq_band]);
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;
    u_int8_t         num_ant_config;

    num_ant_config = 1; /* default antenna configuration */

    if (pBase->version >= 0x0E0D)
        if (pModal->useAnt1)
            num_ant_config += 1;

    return num_ant_config;
#else
    return 1;
#endif
}

HAL_STATUS
ar9300EepromGetAntCfg(struct ath_hal_9300 *ahp, HAL_CHANNEL_INTERNAL *chan,
                   u_int8_t index, u_int16_t *config)
{
#if 0
    ar9300_eeprom_t  *eep = &ahp->ah_eeprom.def;
    MODAL_EEPDEF_HEADER *pModal = &(eep->modalHeader[IS_CHAN_2GHZ(chan)]);
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;

    switch (index) {
        case 0:
            *config = pModal->antCtrlCommon & 0xFFFF;
            return HAL_OK;
        case 1:
            if (pBase->version >= 0x0E0D) {
                if (pModal->useAnt1) {
                    *config = ((pModal->antCtrlCommon & 0xFFFF0000) >> 16);
                    return HAL_OK;
                }
            }
            break;

        default:
            break;
    }
#endif
    return HAL_EINVAL;
}

u_int8_t*
ar9300EepromGetCustData(struct ath_hal_9300 *ahp)
{
    return (u_int8_t *)ahp;
}

#ifdef UNUSED
static inline HAL_STATUS
ar9300CheckEeprom(struct ath_hal *ah)
{
#if 0
    u_int32_t sum = 0, el;
    u_int16_t *eepdata;
    int i;
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_BOOL need_swap = AH_FALSE;
    ar9300_eeprom_t *eep = (ar9300_eeprom_t *)&ahp->ah_eeprom.def;
    u_int16_t magic, magic2;
    int addr;
    u_int16_t temp;

    /*
    ** We need to check the EEPROM data regardless of if it's in flash or
    ** in EEPROM.
    */

    if (!ahp->ah_priv.priv.ah_eepromRead(ah, AR9300_EEPROM_MAGIC_OFFSET, &magic)) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Reading Magic # failed\n", __func__);
        return AH_FALSE;
    }

    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Read Magic = 0x%04X\n", __func__, magic);

    if (!ar9300EepDataInFlash(ah))
    {

        if (magic != AR9300_EEPROM_MAGIC)
        {
            magic2 = SWAP16(magic);

            if (magic2 == AR9300_EEPROM_MAGIC)
            {
                need_swap = AH_TRUE;
                eepdata = (u_int16_t *)(&ahp->ah_eeprom);

                for (addr=0; addr<sizeof(ar9300_eeprom_t)/sizeof(u_int16_t); addr++)
            {
                    temp = SWAP16(*eepdata);
                    *eepdata = temp;
                    eepdata++;

                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "0x%04X  ", *eepdata);
                    if (((addr+1)%6) == 0)
                        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "\n");
            }
        }
            else
        {
                HDPRINTF(ah, HAL_DBG_EEPROM, "Invalid EEPROM Magic. endianness missmatch.\n");
                return HAL_EEBADSUM;
        }
    }
    }
    else
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM being read from flash @0x%p\n", ah->ah_st);
    }

    HDPRINTF(ah, HAL_DBG_EEPROM, "need_swap = %s.\n", need_swap?"True":"False");

    if (need_swap) {
        el = SWAP16(ahp->ah_eeprom.def.baseEepHeader.length);
    } else {
        el = ahp->ah_eeprom.def.baseEepHeader.length;
    }

    eepdata = (u_int16_t *)(&ahp->ah_eeprom.def);
    for (i = 0; i < AH_MIN(el, sizeof(ar9300_eeprom_t))/sizeof(u_int16_t); i++)
        sum ^= *eepdata++;

    if (need_swap) {
        /*
        *  preddy: EEPROM endianness does not match. So change it
        *  8bit values in eeprom data structure does not need to be swapped
        *  Only >8bits (16 & 32) values need to be swapped
        *  If a new 16 or 32 bit field is added to the EEPROM contents,
        *  please make sure to swap the field here
        */
        u_int32_t integer,j;
        u_int16_t word;

        HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM Endianness is not native.. Changing \n");

        /* convert Base Eep header */
        word = SWAP16(eep->baseEepHeader.length);
        eep->baseEepHeader.length = word;

        word = SWAP16(eep->baseEepHeader.checksum);
        eep->baseEepHeader.checksum = word;

        word = SWAP16(eep->baseEepHeader.version);
        eep->baseEepHeader.version = word;

        word = SWAP16(eep->baseEepHeader.regDmn[0]);
        eep->baseEepHeader.regDmn[0] = word;

        word = SWAP16(eep->baseEepHeader.regDmn[1]);
        eep->baseEepHeader.regDmn[1] = word;

        word = SWAP16(eep->baseEepHeader.rfSilent);
        eep->baseEepHeader.rfSilent = word;

        word = SWAP16(eep->baseEepHeader.blueToothOptions);
        eep->baseEepHeader.blueToothOptions = word;

        word = SWAP16(eep->baseEepHeader.deviceCap);
        eep->baseEepHeader.deviceCap = word;

        /* convert Modal Eep header */
        for (j = 0; j < N(eep->modalHeader); j++) {
            MODAL_EEPDEF_HEADER *pModal = &eep->modalHeader[j];
            integer = SWAP32(pModal->antCtrlCommon);
            pModal->antCtrlCommon = integer;

            for (i = 0; i < AR9300_MAX_CHAINS; i++) {
                integer = SWAP32(pModal->antCtrlChain[i]);
                pModal->antCtrlChain[i] = integer;
    }

            for (i = 0; i < AR9300_EEPROM_MODAL_SPURS; i++) {
                word = SWAP16(pModal->spurChans[i].spurChan);
                pModal->spurChans[i].spurChan = word;
            }
            }
        }

    /* Check CRC - Attach should fail on a bad checksum */
    if (sum != 0xffff || owl_get_eepdef_ver(ahp) != AR9300_EEP_VER ||
        owl_get_eepdef_rev(ahp) < AR9300_EEP_NO_BACK_VER) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "Bad EEPROM checksum 0x%x or revision 0x%04x\n",
            sum, owl_get_eepdef_ver(ahp));
        return HAL_EEBADSUM;
        }
#ifdef EEPROM_DUMP
    ar9300EepromDefDump(ah, eep);
#endif


#if 0
#ifdef AH_AR9300_OVRD_TGT_PWR

    /* 14.4 EEPROM contains low target powers.      Hardcode until EEPROM > 14.4 */
    if (owl_get_eepdef_ver(ahp) == 14 && owl_get_eepdef_rev(ahp) <= 4) {
        MODAL_EEPDEF_HEADER *pModal;

#ifdef EEPROM_DUMP
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "Original Target Powers\n");
        ar9300EepDefDumpTgtPower(ah, eep);
#endif
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, 
                "Override Target Powers. EEPROM Version is %d.%d, Device Type %d\n",
                owl_get_eepdef_ver(ahp),
                owl_get_eepdef_rev(ahp),
                eep->baseEepHeader.deviceType);


        ar9300EepDefOverrideTgtPower(ah, eep);

        if (eep->baseEepHeader.deviceType == 5) {
            /* for xb72 only: improve transmit EVM for interop */
            pModal = &eep->modalHeader[1];
            pModal->txFrameToDataStart = 0x23;
            pModal->txFrameToXpaOn = 0x23;
            pModal->txFrameToPaOn = 0x23;
    }

#ifdef EEPROM_DUMP
        HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "Modified Target Powers\n");
        ar9300EepDefDumpTgtPower(ah, eep);
#endif
        }
#endif /* AH_AR9300_OVRD_TGT_PWR */
#endif
#endif
    return HAL_OK;
}
#endif

static u_int16_t
ar9300EepromGetSpurChan(struct ath_hal *ah, u_int16_t i,HAL_BOOL is2GHz)
{
    u_int16_t   spur_val = AR_NO_SPUR;
#if 0
    struct ath_hal_9300 *ahp = AH9300(ah);
    ar9300_eeprom_t *eep = (ar9300_eeprom_t *)&ahp->ah_eeprom;

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
        spur_val = eep->modalHeader[is2GHz].spurChans[i].spurChan;
        break;

    }
#endif
    return spur_val;
}

#ifdef UNUSED
static inline HAL_BOOL
ar9300FillEeprom(struct ath_hal *ah)
{
    return ar9300EepromRestore(ah);
}
#endif

u_int16_t
ar9300EepromStructSize(void) 
{
    return sizeof(ar9300_eeprom_t);
}

int ar9300EepromStructDefaultMany(void)
{
    return sizeof(default9300)/sizeof(default9300[0]);
}


ar9300_eeprom_t *
ar9300EepromStructDefault(int defaultIndex) 
{
    if (defaultIndex>=0 && defaultIndex<sizeof(default9300)/sizeof(default9300[0]))
    {
        return default9300[defaultIndex];
    }
    else
    {
        return 0;
    }
}

ar9300_eeprom_t *
ar9300EepromStructDefaultFindById(int id) 
{
    int it;

    for (it=0; it<sizeof(default9300)/sizeof(default9300[0]); it++)
    {
        if (default9300[it]!=0 && default9300[it]->templateVersion==id)
        {
            return default9300[it];
        }
    }
    return 0;
}


HAL_BOOL
ar9300CalibrationDataReadFlash(struct ath_hal *ah, long address, u_int8_t *buffer, int many)
{
	if (((address)<0)||((address+many)>AR9300_EEPROM_SIZE-1)){
		return AH_FALSE;
	}
#ifndef WIN32
#ifdef ART_BUILD
#ifdef MDK_AP          //MDK_AP is defined only in NART AP build 
    int fd;
    if ((fd = open("/dev/caldata", O_RDWR)) < 0) {
        perror("Could not open flash\n");
        return 1 ;
    }
    lseek(fd, address, SEEK_SET);
    if (read(fd, buffer, many) != many) {
        perror("\nRead\n");
        return 1;
    }
    close(fd);                
    //for (i=0; i<many; i++)
    //{
    //    printf("addr=0x%x, data = %x\n", address+i, 0xff&buffer[i]);
    //}
    return AH_TRUE;
#endif   //MDK_AP
#endif
#endif
	return AH_FALSE;
}

HAL_BOOL
ar9300CalibrationDataReadEeprom(struct ath_hal *ah, long address, u_int8_t *buffer, int many)
{
	int i;
	u_int8_t value[2];
    unsigned long eepAddr;
    unsigned long byteAddr;
    u_int16_t *svalue;
    struct ath_hal_9300 *ahp = AH9300(ah);

	if (((address)<0)||((address+many)>AR9300_EEPROM_SIZE)){
		return AH_FALSE;
	}

    for (i=0;i<many;i++){
        eepAddr = (u_int16_t)(address+i)/2;
        byteAddr = (u_int16_t) (address+i)%2;
        svalue = (u_int16_t *)value;
        if (!ahp->ah_priv.priv.ah_eepromRead(ah, eepAddr, svalue)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read eeprom region \n", __func__);
		    return AH_FALSE;
        }  
        buffer[i] = (*svalue>>(8*byteAddr))&0xff;
    }  
	return AH_TRUE;
}

HAL_BOOL
ar9300CalibrationDataReadOtp(struct ath_hal *ah, long address, u_int8_t *buffer, int many)
{
	int i;
    unsigned long eepAddr;
    unsigned long byteAddr;
    u_int32_t svalue;

	if (((address)<0)||((address+many)>0x400))
	{
		return AH_FALSE;
	}

    for (i=0; i<many; i++)
	{
        eepAddr = (u_int16_t)(address+i)/4;		// otp is 4 bytes long????
        byteAddr = (u_int16_t) (address+i)%4;
        if (!ar9300OtpRead(ah, eepAddr, &svalue)) 
		{
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read otp region \n", __func__);
		    return AH_FALSE;
        }  
        buffer[i] = (svalue>>(8*byteAddr))&0xff;
    }  
	return AH_TRUE;
}

#ifdef ATH_CAL_NAND_FLASH
HAL_BOOL
ar9300CalibrationDataReadNand(struct ath_hal *ah, long address, u_int8_t *buffer, int many)
{
	int ret_len;
	int ret_val = 1;
	
	  /* Calling OS based API to read NAND */
	ret_val = OS_NAND_FLASH_READ(ATH_CAL_NAND_PARTITION, address, many, &ret_len, buffer);
	
	return (ret_val ? AH_FALSE: AH_TRUE);
}
#endif

HAL_BOOL
ar9300CalibrationDataRead(struct ath_hal *ah, long address, u_int8_t *buffer, int many)
{
	switch(AH9300(ah)->CalibrationDataSource)
	{
		case CalibrationDataFlash:
			return ar9300CalibrationDataReadFlash(ah,address,buffer,many);
		case CalibrationDataEeprom:
			return ar9300CalibrationDataReadEeprom(ah,address,buffer,many);
		case CalibrationDataOtp:
			return ar9300CalibrationDataReadOtp(ah,address,buffer,many);
#ifdef ATH_CAL_NAND_FLASH
		case CalibrationDataNand:
			return ar9300CalibrationDataReadNand(ah,address,buffer,many);
#endif
	}

   return AH_FALSE;
}


HAL_BOOL 
ar9300CalibrationDataReadArray(struct ath_hal *ah, int address, u_int8_t *buffer, int many)
{
    int it;

    for (it=0; it<many; it++)
    {
        (void)ar9300CalibrationDataRead(ah, address-it, buffer+it, 1);
    }
    return AH_TRUE;
}


/*
 * the address where the first configuration block is written
 */
static const int BaseAddress=0x3ff;                // 1KB
static const int BaseAddress_512=0x1ff;            // 512Bytes

/*
 * the address where the NAND first configuration block is written
 */
#ifdef ATH_CAL_NAND_FLASH
static const int BaseAddress_Nand = AR9300_FLASH_CAL_START_OFFSET;
#endif


/*
 * the lower limit on configuration data
 */
static const int LowLimit=0x040;

//
// returns size of the physical eeprom in bytes.
// 1024 and 2048 are normal sizes. 
// 0 means there is no eeprom. 
// 
int32_t 
ar9300EepromSize(struct ath_hal *ah)
{
	u_int16_t data;
    //
	// first we'll try for 4096 bytes eeprom
	//
	if(ar9300EepromReadWord(ah, 2047, &data))
	{
		if(data!=0)
		{
			return 4096;
		}
	}
    //
	// then we'll try for 2048 bytes eeprom
	//
	if(ar9300EepromReadWord(ah, 1023, &data))
	{
		if(data!=0)
		{
			return 2048;
		}
	}
    //
	// then we'll try for 1024 bytes eeprom
	//
	if(ar9300EepromReadWord(ah, 511, &data))
	{
		if(data!=0)
		{
			return 1024;
		}
	}
	return 0;
}


/*
 * find top of memory
 */
int
ar9300EepromBaseAddress(struct ath_hal *ah)
{
	int size;

	size=ar9300EepromSize(ah);
	if(size>0)
	{
		return size-1;
	}
	else
	{
        if(AR_SREV_POSEIDON(ah) || AR_SREV_HORNET(ah)) 
		{
		    return BaseAddress_512;
        } 
		else 
		{
		    return BaseAddress;
		}
	}
}

/*
 * find top of memory
 */
int
ar9300EepromVolatile(struct ath_hal *ah)
{
	if(AH9300(ah)->CalibrationDataSource==CalibrationDataOtp)
	{
		return 0;		// no eeprom, use otp
	}
	else
	{
		return 1;		// board has eeprom or flash
	}
}

/*
 * need to change this to look for the pcie data in the low parts of memory
 * cal data needs to stop a few locations above 
 */
int
ar9300EepromLowLimit(struct ath_hal *ah)
{
    return LowLimit;
}

u_int16_t
ar9300CompressionChecksum(u_int8_t *data, int dsize)
{
    int it;
    int checksum=0;

    for (it=0; it<dsize; it++)
    {
        checksum += data[it];
        checksum &= 0xffff;
    }

    return checksum;
}

int
ar9300CompressionHeaderUnpack(u_int8_t *best, int *code, int *reference, int *length, int *major, int *minor)
{
    unsigned long value[4];

    value[0]=best[0];
    value[1]=best[1];
    value[2]=best[2];
    value[3]=best[3];
    *code=((value[0]>>5)&0x0007);
    *reference=(value[0]&0x001f)|((value[1]>>2)&0x0020);
    *length=((value[1]<<4)&0x07f0)|((value[2]>>4)&0x000f);
    *major=(value[2]&0x000f);
    *minor=(value[3]&0x00ff);

    return 4;
}


static HAL_BOOL
ar9300UncompressBlock(struct ath_hal *ah, u_int8_t *mptr, int mdataSize, u_int8_t *block, int size)
{
    int it;
    int spot;
    int offset;
    int length;

    spot=0;
    for (it=0; it<size; it+=(length+2))
    {
        offset=block[it];
        offset&=0xff;
        spot+=offset;
        length=block[it+1];
        length&=0xff;
        if (length>0 && spot>=0 && spot+length<=mdataSize)
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Restore at %d: spot=%d offset=%d length=%d\n", __func__, it, spot, offset, length);
            OS_MEMCPY(&mptr[spot],&block[it+2],length);
            spot+=length;
        }
        else if (length>0)
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Bad restore at %d: spot=%d offset=%d length=%d\n", __func__, it, spot, offset, length);
            return AH_FALSE;
        }
    }
    return AH_TRUE;
}


static int
ar9300EepromRestoreInternalAddress(struct ath_hal *ah, ar9300_eeprom_t *mptr, int mdataSize, int cptr)
{
    u_int8_t word[MOUTPUT]; 
    ar9300_eeprom_t *dptr;      // was uint8
    int code;
    int reference,length,major,minor;
    int osize;
    int it;
    u_int16_t checksum, mchecksum;
    int restored;

	restored=0;
    for (it=0; it<MSTATE; it++)
    {            
        (void)ar9300CalibrationDataReadArray(ah,cptr,word,CompressionHeaderLength);
        if ((word[0]==0 && word[1]==0 && word[2]==0 && word[3]==0) || 
            (word[0]==0xff && word[1]==0xff && word[2]==0xff && word[3]==0xff))
        {
            break;
        }
        ar9300CompressionHeaderUnpack(word, &code, &reference, &length, &major, &minor);
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Found block at %x: code=%d ref=%d length=%d major=%d minor=%d\n", __func__, cptr, code, reference, length, major, minor);
#ifdef DONTUSE
		if (length>=1024)
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Skipping bad header\n", __func__);
            cptr-=CompressionHeaderLength;
            continue;
        }
#endif
        osize=length;                
        (void)ar9300CalibrationDataReadArray(ah,cptr,word,CompressionHeaderLength+osize+CompressionChecksumLength);
        checksum=ar9300CompressionChecksum(&word[CompressionHeaderLength], length);
        mchecksum= word[CompressionHeaderLength+osize]|(word[CompressionHeaderLength+osize+1]<<8);
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: checksum %x %x\n", __func__, checksum, mchecksum);
        if (checksum==mchecksum)
        {
            switch(code)
            {
                case _CompressNone:
                    if (length!=mdataSize)
                    {
                        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: EEPROM structure size mismatch memory=%d eeprom=%d\n", __func__, mdataSize, length);
                        return -1;
                    }
                    OS_MEMCPY((u_int8_t *)mptr,(u_int8_t *)(word+CompressionHeaderLength),length);
                    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: restored eeprom %d: uncompressed, length %d\n", __func__, it, length);
                    restored = 1;
                    break;
#ifdef UNUSED
                case _CompressLzma:
                    if (reference==ReferenceCurrent)
                    {
                        dptr=mptr;
                    }
                    else
                    {
                        dptr=(u_int8_t *)ar9300EepromStructDefaultFindById(reference);
                        if (dptr==0)
                        {
                            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Can't find reference eeprom struct %d\n", __func__, reference);
                            goto done;
                        }
                    }
                    usize= -1;
                    if (usize!=mdataSize)
                    {
                        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: uncompressed data is wrong size %d %d\n", __func__, usize, mdataSize);
                        goto done;
                    }

                    for (ib=0; ib<mdataSize; ib++)
                    {
                        mptr[ib]=dptr[ib]^word[ib+overhead];
                    }
                    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: restored eeprom %d: compressed, reference %d, length %d\n", __func__, it, reference, length);
                    break;
                case _CompressPairs:
                    if (reference==ReferenceCurrent)
                    {
                        dptr=mptr;
                    }
                    else
                    {
                        dptr=(u_int8_t *)ar9300EepromStructDefaultFindById(reference);
                        if (dptr==0)
                        {
                            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Can't find the reference eeprom structure %d\n", __func__, reference);
                            goto done;
                        }
                    }
                    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: restored eeprom %d: pairs, reference %d, length %d, \n", __func__, it, reference, length);
                    break;
#endif
                case _CompressBlock:
                    if (reference==ReferenceCurrent)
                    {
                        dptr=mptr;
                    }
                    else
                    {
                        dptr=ar9300EepromStructDefaultFindById(reference);
                        if (dptr==0)
                        {
                            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cant find reference eeprom struct %d\n", __func__, reference);
                            break;
                        }
						OS_MEMCPY(mptr,dptr,mdataSize);
                    }

                    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: restore eeprom %d: block, reference %d, length %d\n", __func__, it, reference, length);
                    (void)ar9300UncompressBlock(ah,(u_int8_t *)mptr,mdataSize,(u_int8_t *)(word+CompressionHeaderLength),length);
                    restored = 1;
                    break;
                default:
                    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: unknown compression code %d\n", __func__, code);
                    break;
            }
        }
        else
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: skipping block with bad checksum\n", __func__);
        }
        cptr-=(CompressionHeaderLength+osize+CompressionChecksumLength);
    }

	if(!restored)
	{
		cptr= -1;
	}
	return cptr;
}


static int ar9300EepromRestoreFromHost(struct ath_hal *ah, ar9300_eeprom_t *mptr, int mdataSize)
{
	struct ath_hal_9300 *ahp = AH9300(ah);
	int nptr;
	nptr = -1;

#ifndef WIN32
	/* if cal_in_flash is true, the address sent by LMAC to HAL (i.e. ah->ah_st) 
           is corresponding to Flash. so return from here if ar9300EepDataInFlash(ah) returns true */
	if (ar9300EepDataInFlash(ah))
		return nptr;

	if (ah->ah_st != NULL) {
		char *cal_ptr;
		/*
		 * When calibration data is from host, Host will 
		 * copy the compressed data to the predefined DRAM location saved at ah->ah_st
		 */
		ahp->ah_cal_mem = OS_REMAP((uintptr_t)(ah->ah_st), HOST_CALDATA_SIZE);
		if (!ahp->ah_cal_mem)
		{
			HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot remap dram region \n", __func__);
			return nptr;
		}
		cal_ptr = &((char *)(ahp->ah_cal_mem))[AR9300_FLASH_CAL_START_OFFSET];
		OS_MEMCPY(mptr, cal_ptr, mdataSize);
		nptr = mdataSize;
	}
	/* Check the validity of Data from Host */
	if(nptr >= 0)
	{
		if (mptr->eepromVersion==0xff || mptr->templateVersion==0xff || 
			mptr->eepromVersion==0 || mptr->templateVersion==0)
		{
			// The board is uncalibrated
			nptr = -1;
		}
	}
#endif
	return nptr;
}

static int ar9300EepromRestoreFromFlash(struct ath_hal *ah, ar9300_eeprom_t *mptr, int mdataSize)
{
	int nptr;

	nptr= -1;

#ifndef WIN32

#ifndef MDK_AP //MDK_AP is defined only in NART AP build 
    if (ar9300EepDataInFlash(ah)) {
        struct ath_hal_9300 *ahp = AH9300(ah);
        char *cal_ptr;

        ath_hal_printf(ah, "Restoring Cal data from Flash\n");
        /*
         * When calibration data is saved in flash, read
         * uncompressed eeprom structure from flash and return
         */
        // NOT TESTED ??????????
        cal_ptr = &((char *)(ahp->ah_cal_mem))[AR9300_FLASH_CAL_START_OFFSET];
        OS_MEMCPY(mptr, cal_ptr, mdataSize);

	    //ar9300SwapEeprom((ar9300_eeprom_t *)mptr); DONE IN ar9300Restore()
        nptr = mdataSize;
    }
#else
    {
        /*
         * When calibration data is saved in flash, read
         * uncompressed eeprom structure from flash and return
         */
        int fd;
        int offset;
        u_int8_t word[MOUTPUT]; 
        //printf("Reading flash for calibration data\n");
        if ((fd = open("/dev/caldata", O_RDWR)) < 0) {
            perror("Could not open flash\n");
            return -1;
        }
        /*
         * First 0x1000 are reserved for pcie config writes.
         * offset = devIndex*AR9300_EEPROM_SIZE+FLASH_BASE_CALDATA_OFFSET;
         *     Need for boards with more than one radio
         */
        //offset = FLASH_BASE_CALDATA_OFFSET;
        offset = instance*AR9300_EEPROM_SIZE+FLASH_BASE_CALDATA_OFFSET;
        lseek(fd, offset, SEEK_SET);
        if (read(fd, mptr, mdataSize) != mdataSize) {
            perror("\nRead\n");
		    close(fd);
            return -1;
        }
	    nptr = mdataSize;
        close(fd);
    }
#endif

#endif		/* WIN32 */


	if(nptr>=0)
	{
		if (mptr->eepromVersion==0xff || mptr->templateVersion==0xff || mptr->eepromVersion==0 || mptr->templateVersion==0)
		{   
			// The board is uncalibrated
			nptr = -1;
		} 
	}

	return nptr;
}


/*
 * Read the configuration data from the eeprom.
 * The data can be put in any specified memory buffer.
 *
 * Returns -1 on error. 
 * Returns address of next memory location on success.
 */
int
ar9300EepromRestoreInternal(struct ath_hal *ah, ar9300_eeprom_t *mptr, int mdataSize)
{
	int nptr;

	nptr= -1;


	if ((AH9300(ah)->CalibrationDataTry==CalibrationDataNone ||
		AH9300(ah)->CalibrationDataTry==CalibrationDataFromHost) && 
		AH9300(ah)->TryFromHost && nptr<0)
	{
		AH9300(ah)->CalibrationDataSource=CalibrationDataFromHost;
		AH9300(ah)->CalibrationDataSourceAddress=0;
		nptr=ar9300EepromRestoreFromHost(ah, mptr, mdataSize);
		if(nptr<0)
		{
			AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
			AH9300(ah)->CalibrationDataSourceAddress=0;
		}else {
			ath_hal_printf(ah, "Using Cal data from DRAM 0x%x\n",(unsigned int)ah->ah_st);
		}
	}

	if ((AH9300(ah)->CalibrationDataTry==CalibrationDataNone || AH9300(ah)->CalibrationDataTry==CalibrationDataEeprom) && AH9300(ah)->TryEeprom && nptr<0)
	{
		/*
		 * need to look at highest eeprom address as well as at BaseAddress=0x3ff
		 * where we used to write the data
		 */
		AH9300(ah)->CalibrationDataSource=CalibrationDataEeprom;
		if(AH9300(ah)->CalibrationDataTryAddress!=0)
		{
			AH9300(ah)->CalibrationDataSourceAddress=AH9300(ah)->CalibrationDataTryAddress;
			nptr=ar9300EepromRestoreInternalAddress(ah, mptr, mdataSize, AH9300(ah)->CalibrationDataSourceAddress);
		}
		else
		{
			AH9300(ah)->CalibrationDataSourceAddress=ar9300EepromBaseAddress(ah);
			nptr=ar9300EepromRestoreInternalAddress(ah, mptr, mdataSize, AH9300(ah)->CalibrationDataSourceAddress);
			if(nptr<0)
			{
				if(AH9300(ah)->CalibrationDataSourceAddress!=BaseAddress)
				{
					AH9300(ah)->CalibrationDataSourceAddress=BaseAddress;
					nptr=ar9300EepromRestoreInternalAddress(ah, mptr, mdataSize, AH9300(ah)->CalibrationDataSourceAddress);
				}
			}
		}
		if(nptr<0)
		{
			AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
			AH9300(ah)->CalibrationDataSourceAddress=0;
		}else {
			ath_hal_printf(ah, "Using Cal data from EEPROM 0x%x\n",(unsigned int)AH9300(ah)->CalibrationDataSourceAddress);
		}
	}

	// ##### should be an ifdef test for any AP usage, either in driver or in nart
	if ((AH9300(ah)->CalibrationDataTry==CalibrationDataNone || AH9300(ah)->CalibrationDataTry==CalibrationDataFlash) && AH9300(ah)->TryFlash && nptr<0)
	{
		AH9300(ah)->CalibrationDataSource=CalibrationDataFlash;
		AH9300(ah)->CalibrationDataSourceAddress=0;		// how are we supposed to set this for flash?
		nptr=ar9300EepromRestoreFromFlash(ah, mptr, mdataSize);
		if(nptr<0)
		{
			AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
			AH9300(ah)->CalibrationDataSourceAddress=0;
		}else {
			ath_hal_printf(ah, "Using Cal data from Flash 0x%x\n",(unsigned int)ah->ah_st);
		}
	}
	if ((AH9300(ah)->CalibrationDataTry==CalibrationDataNone || AH9300(ah)->CalibrationDataTry==CalibrationDataOtp) && AH9300(ah)->TryOtp && nptr<0)
	{
		AH9300(ah)->CalibrationDataSource=CalibrationDataOtp;
		if(AR_SREV_POSEIDON(ah) || AR_SREV_HORNET(ah)) {
			AH9300(ah)->CalibrationDataSourceAddress=BaseAddress_512;
        	} else {
			AH9300(ah)->CalibrationDataSourceAddress=BaseAddress;
		}
		nptr=ar9300EepromRestoreInternalAddress(ah, mptr, mdataSize, AH9300(ah)->CalibrationDataSourceAddress);
		if(nptr<0)
		{
			AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
			AH9300(ah)->CalibrationDataSourceAddress=0;
		}else {
			ath_hal_printf(ah, "Using Cal data from OTP memory\n");
		}
	}

#ifdef ATH_CAL_NAND_FLASH
	if ((AH9300(ah)->CalibrationDataTry==CalibrationDataNone || AH9300(ah)->CalibrationDataTry==CalibrationDataNand) && AH9300(ah)->TryNand && nptr<0)
	{
		AH9300(ah)->CalibrationDataSource=CalibrationDataNand;
		AH9300(ah)->CalibrationDataSourceAddress= ((unsigned int)ah->ah_st) + BaseAddress_Nand;

		if(ar9300CalibrationDataRead(ah, AH9300(ah)->CalibrationDataSourceAddress, (u_int8_t *)mptr, mdataSize) == AH_TRUE)
		{
		    nptr = mdataSize;
		}
		/*nptr=ar9300EepromRestoreInternalAddress(ah, mptr, mdataSize, AH9300(ah)->CalibrationDataSourceAddress);*/
		if(nptr<0)
		{
			AH9300(ah)->CalibrationDataSource=CalibrationDataNone;
			AH9300(ah)->CalibrationDataSourceAddress=0;
		}else {
			ath_hal_printf(ah, "Using Cal data from NAND memory\n");
		}
	}
#endif

	if (nptr < 0)
	{
		ath_hal_printf(ah, "Using Cal data from Driver Default Settings\n");
		nptr = ar9300EepromRestoreSomething(ah, mptr, mdataSize);
	}

	return nptr;
}
	

/******************************************************************************/
/*!
**  \brief Eeprom Swapping Function
**
**  This function will swap the contents of the "longer" EEPROM data items
**  to ensure they are consistent with the endian requirements for the platform
**  they are being compiled for
**
**  \param eh	Pointer to the EEPROM data structure
**  \return N/A
*/

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
void
ar9300SwapEeprom(ar9300_eeprom_t *eep)
{
    u_int32_t dword;
    u_int16_t word;
    int	      i;

    word = __bswap16(eep->baseEepHeader.regDmn[0]);
    eep->baseEepHeader.regDmn[0] = word;

    word = __bswap16(eep->baseEepHeader.regDmn[1]);
    eep->baseEepHeader.regDmn[1] = word;

    dword = __bswap32(eep->baseEepHeader.swreg);
    eep->baseEepHeader.swreg = dword;

    dword = __bswap32(eep->modalHeader2G.antCtrlCommon);
    eep->modalHeader2G.antCtrlCommon = dword;

    dword = __bswap32(eep->modalHeader2G.antCtrlCommon2);
    eep->modalHeader2G.antCtrlCommon2 = dword;

    dword = __bswap32(eep->modalHeader2G.papdRateMaskHt20);
    eep->modalHeader2G.papdRateMaskHt20 = dword;

    dword = __bswap32(eep->modalHeader2G.papdRateMaskHt40);
    eep->modalHeader2G.papdRateMaskHt40 = dword;

    dword = __bswap32(eep->modalHeader5G.antCtrlCommon);
    eep->modalHeader5G.antCtrlCommon = dword;

    dword = __bswap32(eep->modalHeader5G.antCtrlCommon2);
    eep->modalHeader5G.antCtrlCommon2 = dword;

    dword = __bswap32(eep->modalHeader5G.papdRateMaskHt20);
    eep->modalHeader5G.papdRateMaskHt20 = dword;

    dword = __bswap32(eep->modalHeader5G.papdRateMaskHt40);
    eep->modalHeader5G.papdRateMaskHt40 = dword;

    for (i=0;i<OSPREY_MAX_CHAINS;i++)
    {
        word = __bswap16(eep->modalHeader2G.antCtrlChain[i]);
		eep->modalHeader2G.antCtrlChain[i] = word;

		word = __bswap16(eep->modalHeader5G.antCtrlChain[i]);
		eep->modalHeader5G.antCtrlChain[i] = word;
	}
}


void ar9300EepromTemplateSwap(void)
{
    int it;
    ar9300_eeprom_t *dptr;

    for (it=0; it<sizeof(default9300)/sizeof(default9300[0]); it++)
    {
        dptr=ar9300EepromStructDefault(it);
        if (dptr != 0)
		{
            ar9300SwapEeprom(dptr);
        }
    }
}
#endif


/*
 * Restore the configuration structure by reading the eeprom.
 * This function destroys any existing in-memory structure content.
 */
HAL_BOOL
ar9300EepromRestore(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    ar9300_eeprom_t *mptr;
    int mdataSize;
    HAL_BOOL status = AH_FALSE;

    mptr = &ahp->ah_eeprom;
    mdataSize = ar9300EepromStructSize();

    if (mptr!=0 && mdataSize>0)
    {
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
        ar9300EepromTemplateSwap();
        ar9300SwapEeprom(mptr);
#endif
        //
        // At this point, mptr points to the eeprom data structure in it's "default"
        // state.  If this is big endian, swap the data structures back to "little endian"
        // form.
        //
        if (ar9300EepromRestoreInternal(ah,mptr,mdataSize)>=0)
        {
            status = AH_TRUE;
        }

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
        // Second Swap, back to Big Endian
        ar9300EepromTemplateSwap();
        ar9300SwapEeprom(mptr);
#endif

    }
    ahp->ah_2G_papdRateMaskHt40 = mptr->modalHeader2G.papdRateMaskHt40;
    ahp->ah_2G_papdRateMaskHt20 = mptr->modalHeader2G.papdRateMaskHt20;
    ahp->ah_5G_papdRateMaskHt40 = mptr->modalHeader5G.papdRateMaskHt40;
    ahp->ah_5G_papdRateMaskHt20 = mptr->modalHeader5G.papdRateMaskHt20;
    return status;
}

int32_t ar9300ThermometerGet(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
	int thermometer= (ahp->ah_eeprom.baseEepHeader.miscConfiguration>>1)&0x3;
	thermometer--;
	return thermometer;
}


HAL_BOOL ar9300ThermometerApply(struct ath_hal *ah)
{
	int thermometer=ar9300ThermometerGet(ah);

/* ch0_RXTX4 */
//#define AR_PHY_65NM_CH0_RXTX4       AR_PHY_65NM(ch0_RXTX4)
#define AR_PHY_65NM_CH1_RXTX4       AR_PHY_65NM(ch1_RXTX4)
#define AR_PHY_65NM_CH2_RXTX4       AR_PHY_65NM(ch2_RXTX4)
//#define AR_PHY_65NM_CH0_RXTX4_THERM_ON          0x10000000
//#define AR_PHY_65NM_CH0_RXTX4_THERM_ON_S        28
#define AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR_S        29
#define AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR         (0x1<<AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR_S)

	if(thermometer<0)
	{
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR, 0);
        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR, 0);
	        if (!AR_SREV_WASP(ah)) {
			    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR, 0);
	        }
      	}
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	        if (!AR_SREV_WASP(ah)) {
			    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	        }
      	}
	}
	else
	{
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR, 1);
        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR, 1);
	        if (!AR_SREV_WASP(ah)) {
			    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON_OVR, 1);
	        }
      	}
		if(thermometer==0)
		{
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 1);
	        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
				OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	            if (!AR_SREV_WASP(ah)) {
				    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	            }
          	}
		}
		else if(thermometer==1)
		{
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
				OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 1);
	            if (!AR_SREV_WASP(ah)) {
				    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	            }
          	}
		}
		else if(thermometer==2)
		{
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
				OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 0);
	            if (!AR_SREV_WASP(ah)) {
				    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX4, AR_PHY_65NM_CH0_RXTX4_THERM_ON, 1);
	            }
          	}
		}
	}
	return AH_TRUE;
}

int32_t ar9300TuningCapsParamsGet(struct ath_hal *ah)
{
	int TuningCapsParams;

    ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	TuningCapsParams = eep->baseEepHeader.params_for_tuning_caps[0];

	return TuningCapsParams;
}

/*
*	Read the tuning caps params from eeprom and set to correct register.
*	To regulation the frequency accuracy.
*/
HAL_BOOL ar9300TuningCapsApply(struct ath_hal *ah)
{
	int TuningCapsParams;
	ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;
	
	TuningCapsParams=ar9300TuningCapsParamsGet(ah);
	if((eep->baseEepHeader.featureEnable & 0x40)>>6)
	{
		TuningCapsParams &= 0x7f;

		if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
			OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_XTAL, AR_HORNET_CHO_XTAL_CAPINDAC, TuningCapsParams);
			OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_XTAL, AR_HORNET_CHO_XTAL_CAPOUTDAC, TuningCapsParams);
		} else {
			//
			//someone needs to check that this is correct for oprey
			//
			OS_REG_RMW_FIELD(ah, 0x16294, AR_HORNET_CHO_XTAL_CAPINDAC, TuningCapsParams);
			OS_REG_RMW_FIELD(ah, 0x16294, AR_HORNET_CHO_XTAL_CAPOUTDAC, TuningCapsParams);
		}
	}

	return AH_TRUE;	
}


/*
 * Read EEPROM header info and program the device for correct operation
 * given the channel value.
 */
HAL_BOOL
ar9300EepromSetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    ar9300XpaBiasLevelApply(ah, IS_CHAN_2GHZ(chan));

    ar9300AntCtrlApply(ah, IS_CHAN_2GHZ(chan));
    ar9300DriveStrengthApply(ah);

	/* wait for Poseidon internal regular turnning */
    /* for Hornet we move it before initPLL to avoid an access issue */
    #ifndef AR9485_EMULATION
    /* Function not used when EMULATION. */
    if(!AR_SREV_HORNET(ah) && !AR_SREV_WASP(ah)) {
        ar9300InternalRegulatorApply(ah);
    }
    #endif

    ar9300AttenuationApply(ah, chan->channel);

	ar9300QuickDropApply(ah, chan->channel);

    ar9300ThermometerApply(ah);

    if(!AR_SREV_WASP(ah))
    {
	    ar9300TuningCapsApply(ah);
    }
	
	ar9300TxEndToXpabOffApply(ah, chan->channel);

    return AH_TRUE;
}


u_int8_t *
ar9300EepromGetSpurChansPtr(struct ath_hal *ah, HAL_BOOL is2GHz)
{
   ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;

   if (is2GHz)
   {
       return &(eep->modalHeader2G.spurChans[0]);
   }
   else
   {
       return &(eep->modalHeader5G.spurChans[0]);
   }
}
   
#undef N
#endif /* AH_SUPPORT_AR9300 */

