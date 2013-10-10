/*************************************************************************/
/*  Copyright (c) 2008 Atheros Communications, Inc., All Rights Reserved */
/*                                                                       */
/*  Module Name : otp.c                                                  */
/*                                                                       */
/*  Abstract                                                             */
/*      This file contains OTP code.                                     */
/*                                                                       */
/*  NOTES                                                                */
/*      None                                                             */
/*                                                                       */
/*************************************************************************/

#include <dv_dbg.h>
#include <asm/addrspace.h>
#include <asm/types.h>
#include <config.h>
#include <ar7240_soc.h>
#include "wasp_api.h"
#include "otp_api.h"

#define OTP_SIZE        (0x1000-8)

#define _range_check(addr) if (addr > OTP_SIZE) return 0xdeadbeef;

u32 otp_init_flag = 0;

#if !CONFIG_WASP_EMU_NO_OTP
LOCAL void
mem_shift(u32 *dst, u32 shift, u32 len)
{
    u8 *pDst = (u8 *)dst;
    u8 *pSrc = (u8 *)(dst+shift);
    u32 i=0;

    for(i=0; i<len; i++)
        *pDst = *pSrc;
}
#endif

LOCAL u32
otp_compute_cksum(u32 *ptr, int len)
{
    unsigned int sum=0x0;
    int i=0;

    for (i=0; i<(len/4); i++)
        sum = sum ^ ptr[i];

    return sum;
}


/*! otp_read_patch
 *
 *  return the patch offset if exist
 */
LOCAL u32
otp_read_patch(void)
{
    u32 pAddr = 0x0;
    u16 i=0;
    u8 off_tbl[4] = {0};

    /* Assume the offset table is at the first data region */
    if( 0xdeadbeef == A_OTP_COPY((u32 *)off_tbl, (u32*)OTP_USB_TBL_OFFSET, 4) )
        goto ERR_DONE;

    /* Check if the offset table exist */
    for(i=3; i>=2; i--) {
        if(off_tbl[i]!=0xff && off_tbl[i]!=0x0) {
            /* Inspect the information header */
            pAddr = (u32)(off_tbl[i]*2);
            break;
        }
    }

ERR_DONE:
    return pAddr;
}

/*! otp_get_usb_bias - the USB phy bias compensation data
 *
 *  return - 0x3 for the default value
 *         - others, not more than 0xf, [25:22]
 *
 */
LOCAL u8
otp_get_usb_bias(void)
{
    /* USB phy bias correct value is 0~0xF */
    u8 buf[4] = {0};
    u8 usb_bias = 3;
    u8 value=0;

    DV_DBG_RECORD_LOCATION(OTP_C);
    /* If art calibration data is available, bit 7 will set to 1 if this is a valid data */
    if( 0xdeadbeef == A_OTP_COPY((u32 *)buf, (u32*)OTP_USB_BIAS_OFFSET, 4) )
        goto ERR_DONE;

    value = buf[0]&~OTP_CAL_INDICATOR;
    if( (value<=0xf) && (buf[0] & OTP_CAL_INDICATOR) ) {
        usb_bias = value;
        return usb_bias;
     }

    /* If no ART data and ATE calibration data is available,
     * check value located in OTP static region
     */
    if( 0xdeadbeef == A_OTP_COPY((u32 *)buf, (u32*)OTP_STATIC_USB_BIAS_OFFSET, 4) )
        goto ERR_DONE;

    if( buf[0]<=0xf )  {
        DV_DBG_RECORD_LOCATION(OTP_C);
        usb_bias = buf[0];
        return usb_bias;
     }

ERR_DONE:
     DV_DBG_RECORD_LOCATION(OTP_C);
     /* Return default value */
     return usb_bias;
}

LOCAL u8
otp_get_mdio_phy_addr(void)
{
    /* MDIO phy addr correct value is 1~7 */
    u8 buf[4] = {0};
    u8 paddr = 7;
    u8 value=0;

    DV_DBG_RECORD_LOCATION(OTP_C);

    /* If art calibration data is available, bit 7 will set to 1 if this is a valid data */
    if( 0xdeadbeef == A_OTP_COPY((u32 *)buf, (u32*)OTP_USB_BIAS_OFFSET, 4) )
        goto ERR_DONE;

    value=buf[1]&~OTP_CAL_INDICATOR;
    if( (value>0) && (value<=0x7) && (buf[1] & OTP_CAL_INDICATOR) ) {
        paddr = value;
        return paddr;
    }

    /* If no ART data and ATE calibration data is available,
     * check value located in OTP static region
     */
    if( 0xdeadbeef == A_OTP_COPY((u32 *)buf, (u32*)OTP_STATIC_USB_BIAS_OFFSET, 4) )
        goto ERR_DONE;

    if( (buf[1]>0) && (buf[1]<=0x7) ) {
        paddr = buf[1];
        return paddr;
    }

ERR_DONE:
    DV_DBG_RECORD_LOCATION(OTP_C);
    /* Return default value */
     return paddr;
}

/*!
 * otp_get_usb_info
 *
 */
LOCAL u8 *otp_get_usb_info(void)
{
    u8 *pData = NULL;
    u8 off_tbl[4] = {0};
    long i=0;
    struct otp_usb_hdr img_hdr;

    /* Assume the offset table is at the first data region */
    DV_DBG_RECORD_LOCATION(OTP_C);
    DV_DBG_RECORD_DATA(OTP_USB_TBL_OFFSET);
    DV_DBG_RECORD_DATA(A_OTP_READ(OTP_USB_TBL_OFFSET));

    if( 0xdeadbeef == A_OTP_COPY((u32 *)off_tbl, (u32*)OTP_USB_TBL_OFFSET, 4) )
        goto ERR_DONE;

    DV_DBG_RECORD_DATA(off_tbl[0]);
    DV_DBG_RECORD_DATA(off_tbl[1]);
    DV_DBG_RECORD_DATA(off_tbl[2]);
    DV_DBG_RECORD_DATA(off_tbl[3]);

    /* Check if the offset table exist */
    for(i=1; i>-1; i--) {
        if(off_tbl[i]!=0xff && off_tbl[i]!=0x0) {
            /* Inspect the information header */
            A_PRINTF("offset[%d]: %d\n\r", i, off_tbl[i]*2);

            if( 0xdeadbeef == A_OTP_COPY((u32 *)&img_hdr, (u32 *)(OTP_BASE+(off_tbl[i]*2)), sizeof(struct otp_usb_hdr)) )
                goto ERR_DONE;

            if( img_hdr.len!=0 && img_hdr.len!=0xff ) {
                unsigned long sum = 0;
                pData = (u8 *)OTP_SRAM_BASE;
                //Copy Data include header
                if( 0xdeadbeef == A_OTP_COPY((u32 *)pData, (u32 *)(OTP_BASE+(off_tbl[i]*2)), img_hdr.len) ){
                    pData = NULL;
                    goto ERR_DONE;
                }

                //Calculate Checksum including header
                sum = A_CHKSUM((u32 *)pData, img_hdr.len);
                if(sum) {
                    pData = NULL;
                    A_PRINTF("checksum fail: %08x\n\r", sum);
                    DV_DBG_RECORD_LOCATION(OTP_C);
                    DV_DBG_RECORD_DATA(sum);
                }
                else
                {
                    //Return pointer to USB info starting address (not include header)
                    pData+=4;
                    break;
                }
            }
        }
    }

ERR_DONE:
    return pData;
}

int
otp_get_nand_table(u8 *tbl, u8 len)
{
	DV_DBG_RECORD_LOCATION(OTP_C);
	DV_DBG_RECORD_DATA(OTP_NAND_TBL_OFFSET);

	if (0xdeadbeef == A_OTP_COPY((u32 *)tbl, (u32*)OTP_NAND_TBL_OFFSET, len)) {
		A_UART_PUTS("otp_get_nand_table failed\n");
		return -1;
	}

	A_UART_PUTS("otp_get_nand_table: ");
	for (; len; len --, tbl ++) {
		A_PRINTF("0x%x ", *tbl);
	}
	A_PRINTF("\n");

	return 0;
}

/*!
 * otp_copy
 *
 * read otp information from "src" offset to "dst" buffer with "len" bytes
 *
 */
    LOCAL u32
otp_copy(u32 *dst, u32 *src, long len)
{
    long size = len; // round to alignment

    /* test by copying from SRAM */
#if CONFIG_WASP_EMU_NO_OTP
    u8 *d, *s;

    //shift = (u32)src % 4;
    //A_PRINTF("otp_copy data... from %x to %x, need %d shift \n\r", src, dst, shift);

    d = (u8 *)dst;
    s = (u8 *)src;

    while(size--) {
        *d++ = *s++;
    }
#else
    unsigned long shift = 0;
    long i=0;
    unsigned long s;

    if (!otp_init_flag)
        return 0xdeadbeef;

    shift = (u32)src % 4;

    /* make sure the offset is aligned */
    s = (shift==0)?(u32)src:(((u32)src) & 0xfc);

    /* read data from otp in word size */
    for(i=0; i<size; i+=4)
        dst[i/4] = A_OTP_READ(s+i);

    /* move data to align 4 bytes */
    if( shift!=0 )
        mem_shift(dst, shift, len);

#endif
    return 1;
}

/*!
 * init
 *  Note : when AHB clock rate is changed, this API must be called again
 */
    LOCAL void
otp_init(void)
{
#if CONFIG_WASP_EMU_NO_OTP
    otp_init_flag = 1;
#else
    u32     rdata, tdata;
    u32     ref_freq;

    otp_init_flag = 0;

#if 0
    /* Clear WLAN reset bit */
    rdata = ar7240_reg_rd(AR7240_RESET);
    rdata &= (~AR7240_RESET_WLAN);
    ar7240_reg_wr(AR7240_RESET, rdata);
#endif

    DV_DBG_RECORD_LOCATION(OTP_C);
    /* Get AHB clock rate */

    if ((ar7240_reg_rd(WASP_BOOTSTRAP_REG) & WASP_REF_CLK_25) == 0) {
        ref_freq = 25;
    } else {
        ref_freq = 40;
    }

    /*
     * Calculate how many nanoseconds does a ref clock cycle take
     *           40MHz -> 25 ns
     *           25MHz -> 40 ns
     */
    tdata = 1000 / ref_freq;

    /*
     * Set OTP registers : Set OTP_INTF2_PG_STROBE_PW_REG_V
     *                     OTP_INTF2_PG_STROBE_PW_REG_V * (how many nanoseconds does a ref clock cycle take) = 5000 ns
     *           40MHz -> 25 ns  --> 5000 / 25 = 1000 (0xC8)
     *           25MHz -> 40 ns  --> 5000 / 40 = 1000 (0x7D)
     */
    tdata = 5000 / tdata;
    rdata = otp_reg_read(OTP_INTF2_ADDRESS);
    rdata &= ~OTP_INTF2_PG_STROBE_PW_REG_V_MASK;
    otp_reg_write(OTP_INTF2_ADDRESS,
            rdata |OTP_INTF2_PG_STROBE_PW_REG_V_SET(tdata));

    /*
     * Set OTP registers : Set OTP_INTF3_RD_STROBE_PW_REG_V to 8
     */
    rdata = otp_reg_read(OTP_INTF3_ADDRESS);
    rdata &= ~OTP_INTF3_RD_STROBE_PW_REG_V_MASK;
    otp_reg_write(OTP_INTF3_ADDRESS,
            rdata | OTP_INTF3_RD_STROBE_PW_REG_V_SET(8));

    /*
     * Set OTP registers : Set OTP_PGENB_SETUP_HOLD_TIME to 3
     */
    rdata = otp_reg_read(OTP_PGENB_SETUP_HOLD_TIME_ADDRESS);
    rdata &= ~OTP_PGENB_SETUP_HOLD_TIME_DELAY_MASK;
    otp_reg_write(OTP_PGENB_SETUP_HOLD_TIME_ADDRESS,
            rdata|OTP_PGENB_SETUP_HOLD_TIME_DELAY_SET(3));

    otp_init_flag = 1;

#endif
    DV_DBG_RECORD_LOCATION(OTP_C);
}

/*!
 * read
 *  Note : addr must be a multiple of 4
 *         For a 512-byte OTP, legal value is 0, 4, 8, ... 508
 */
    LOCAL u32
otp_read(u32 addr)
{
    u32     rdata;
    u32     i=0;

#if 0
    _range_check(addr);
#else
	addr -= OTP_BASE;
#endif

    if (!otp_init_flag)
        return 0xdeadbeef;

    rdata = otp_reg_read(OTP_MEM_ADDRESS + addr);

    do {
        if(i++>65536) goto ERR_DONE;
        udelay(1);
        rdata = otp_reg_read(OTP_STATUS0_ADDRESS);
    } while(OTP_STATUS0_EFUSE_ACCESS_BUSY_GET(rdata) ||
	    !OTP_STATUS0_EFUSE_READ_DATA_VALID_GET(rdata));

    return (otp_reg_read(OTP_STATUS1_ADDRESS));
ERR_DONE:
    return 0xdeadbeef;
}

/*!
 * write
 *  Note : addr must be a multiple of 4
 *         For a 512-byte OTP, legal value is 0, 4, 8, ... 508
 */
    LOCAL u32
otp_write(u32 addr, u32 data)
{
#if 0
    u32     rdata;
    u32     i=0;

    _range_check(addr);

    if (!otp_init_flag)
        return 0xdeadbeef;

    otp_reg_write(OTP_MEM_ADDRESS + addr, data);

    do {
        if(i++>65536) goto ERR_DONE;
        udelay(1);
        rdata = otp_reg_read(OTP_STATUS0_ADDRESS);
    } while(OTP_STATUS0_EFUSE_ACCESS_BUSY_GET(rdata));

    return 0;
ERR_DONE:
#endif
    return 0xdeadbeef;
}

    void
otp_module_install(struct otp_api *api)
{
	api->_otp_init			= otp_init;
	api->_otp_read			= otp_read;
	api->_otp_write			= otp_write;
	api->_otp_read_usb		= otp_get_usb_info;
	api->_otp_read_mdio_paddr	= otp_get_mdio_phy_addr;
	api->_otp_read_patch		= otp_read_patch;
	api->_otp_read_usb_bias		= otp_get_usb_bias;
	api->_otp_chksum		= otp_compute_cksum;
	api->_otp_copy			= otp_copy;
	api->_otp_get_nand_table	= otp_get_nand_table;
}
