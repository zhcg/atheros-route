#ifndef _OTP_API_H
#define _OTP_API_H

#include <asm/types.h>
#include "otp_reg.h"
#include <config.h>

#define OTP_SRAM_BASE            0xBD000700

#if !CONFIG_WASP_EMU_NO_OTP
#define OTP_BASE                 0
#else
#define OTP_BASE                 0xBD000400
#endif
#define OTP_STATIC_USB_BIAS_OFFSET	(OTP_BASE + 0x4)
#define OTP_STATIC_MDIO_PHY_OFFSET	(OTP_BASE + 0x5)
#define OTP_PCIE_OFFSET			(OTP_BASE + 0xc)
#define OTP_USB_TBL_OFFSET		(OTP_BASE + 0x10)
#define OTP_USB_BIAS_OFFSET		(OTP_BASE + 0x14)
#define OTP_MDIO_PHY_OFFSET		(OTP_BASE + 0x15)
#define OTP_NAND_TBL_OFFSET		(OTP_BASE + 0x18)

#define OTP_CAL_INDICATOR		(1 << 7)
#define OTP_NUM_NAND_ENTRIES		2
/*
 *        OTP format
 *
 *        0               1               2               3
 *        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-----------
 * 0x0   |                           Reserved                            |     ^
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+     |
 * 0x4   | USB bias data | MDIO phy addr |                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   Static
 * 0x8   |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+     |
 * 0xC   |                           Reserved                            |     V
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-----------
 * 0x10  |          USB info index       |        Patch code Index       |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0x14  |I| USB phybias |I| MDIO phyaddr|                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0x18  | NAND Vend ID  | NAND Dev ID   |  Byte 3 data  |No. of Addr Cyc|
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0x1c  | NAND Vend ID  | NAND Dev ID   |  Byte 3 data  |No. of Addr Cyc|
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

struct otp_usb_hdr {
    u8 len;
    u8 pad1;
    u8 pad2;
    u8 pad3;
};

struct otp_api {
    void (*_otp_init)(void);
    u32  (*_otp_read)(u32 addr);
    u32  (*_otp_write)(u32 addr, u32 data);
    u8*  (*_otp_read_usb)(void);
    u8   (*_otp_read_mdio_paddr)(void);
    u32  (*_otp_read_patch)(void);
    u8   (*_otp_read_usb_bias)(void);
    u32  (*_otp_chksum)(u32 *, int);
    u32  (*_otp_copy)(u32 *, u32 *, long);
	int	(*_otp_get_nand_table)(u8 *, u8);
};

void
otp_module_install(struct otp_api *api);

#endif

