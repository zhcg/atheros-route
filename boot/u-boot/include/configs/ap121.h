/*
 * This file contains the configuration parameters for the dbau1x00 board.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <configs/ar7240.h>
#include <config.h>
/*-----------------------------------------------------------------------
 * FLASH and environment organization
 *-----------------------------------------------------------------------
 */
 
#if (FLASH_SIZE == 4)
#define CFG_MAX_FLASH_BANKS     1	    /* max number of memory banks */
#define CFG_MAX_FLASH_SECT      64    /* max number of sectors on one chip */
#define CFG_FLASH_SECTOR_SIZE   (64*1024)
#define CFG_FLASH_SIZE          0x00400000 /* Total flash size */

#define ENABLE_DYNAMIC_CONF 1

#else
/* For 2 MB flash */
#define CFG_MAX_FLASH_BANKS     1	    /* max number of memory banks */
#define CFG_MAX_FLASH_SECT      32     /* max number of sectors on one chip */
#define CFG_FLASH_SECTOR_SIZE   (64*1024)
#define CFG_FLASH_SIZE          0x00200000 /* Total flash size */
#endif

#if (CFG_MAX_FLASH_SECT * CFG_FLASH_SECTOR_SIZE) != CFG_FLASH_SIZE
#	error "Invalid flash configuration"
#endif

#define CFG_FLASH_WORD_SIZE     unsigned short 

/* 
 * We boot from this flash
 */
#define CFG_FLASH_BASE		    0x9f000000

#ifdef COMPRESSED_UBOOT
#define BOOTSTRAP_TEXT_BASE         CFG_FLASH_BASE
#define BOOTSTRAP_CFG_MONITOR_BASE  BOOTSTRAP_TEXT_BASE
#endif

/*
 * Defines to change flash size on reboot
 */
#ifdef ENABLE_DYNAMIC_CONF
#define UBOOT_FLASH_SIZE          (256 * 1024)
#define UBOOT_ENV_SEC_START        (CFG_FLASH_BASE + UBOOT_FLASH_SIZE)

#define CFG_FLASH_MAGIC           0xaabacada
#define CFG_FLASH_MAGIC_F         (UBOOT_ENV_SEC_START + CFG_FLASH_SECTOR_SIZE - 0x20)
#define CFG_FLASH_SECTOR_SIZE_F   *(volatile int *)(CFG_FLASH_MAGIC_F + 0x4)
#define CFG_FLASH_SIZE_F          *(volatile int *)(CFG_FLASH_MAGIC_F + 0x8) /* Total flash size */
#define CFG_MAX_FLASH_SECT_F      (CFG_FLASH_SIZE / CFG_FLASH_SECTOR_SIZE) /* max number of sectors on one chip */
#endif


/* 
 * The following #defines are needed to get flash environment right 
 */
#define	CFG_MONITOR_BASE	TEXT_BASE
#define	CFG_MONITOR_LEN		(192 << 10)

#undef CONFIG_BOOTARGS
/* XXX - putting rootfs in last partition results in jffs errors */

/* default mtd partition table */
#undef MTDPARTS_DEFAULT

#if (FLASH_SIZE == 4)
#define	CONFIG_BOOTARGS     "console=ttyS0,115200 root=31:02 rootfstype=squashfs init=/sbin/init mtdparts=ar7240-nor0:256k(u-boot),64k(u-boot-env),2752k(rootfs),896k(uImage),64k(NVRAM),64k(ART)"
#define MTDPARTS_DEFAULT    "mtdparts=ar7240-nor0:256k(u-boot),64k(u-boot-env),2752k(rootfs),896k(uImage),64k(NVRAM),64k(ART)"
#else
#define	CONFIG_BOOTARGS     "console=ttyS0,115200 root=31:01 rootfstype=squashfs init=/sbin/init mtdparts=ar7240-nor0:64k(u-boot),1216k(rootfs),640k(uImage),64k(NVRAM),64k(ART)"
#define MTDPARTS_DEFAULT    "mtdparts=ar7240-nor0:64k(u-boot),1152k(rootfs),704k(uImage),64k(NVRAM),64k(ART)"
#endif

#if (BOARD_STRING == 1)
#undef CONFIG_BOOTARGS
#undef MTDPARTS_DEFAULT
#define	CONFIG_BOOTARGS     "console=ttyS0,115200 root=31:01 rootfstype=squashfs init=/sbin/init mtdparts=ar7240-nor0:64k(u-boot),3008k(rootfs),896k(uImage),64k(mib0),64k(ART)"
#define MTDPARTS_DEFAULT    "mtdparts=ar7240-nor0:64k(u-boot),3008k(rootfs),896k(uImage),64k(NVRAM),64k(ART)"
#endif

#undef CFG_PLL_FREQ

#ifdef CONFIG_HORNET_EMU
    #ifdef CONFIG_HORNET_EMU_HARDI_WLAN
    #define CFG_PLL_FREQ	CFG_PLL_48_48_24
    #else
    #define CFG_PLL_FREQ	CFG_PLL_80_80_40
    #endif
#else
//#define CFG_PLL_FREQ	CFG_PLL_300_300_150
#define CFG_PLL_FREQ	CFG_PLL_400_400_200
#endif

#if CONFIG_40MHZ_XTAL_SUPPORT
    #define CPU_PLL_KI_VAL  4
    #define CPU_PLL_KD_VAL  120
#else
    #define CPU_PLL_KI_VAL  4
    #define CPU_PLL_KD_VAL  75
#endif

#define CPU_PLL_PHASE_SHIFT_VAL     6

/* 3 for CAS latency 3, 2 for for CAS latency 2, other for for CAS latency 2.5 */
#define DDR_CAS_LATENCY_VAL         3

/* Default 1 */
#define CPU_PLL_REFDIV  1

#undef CFG_HZ
/*
 * MIPS32 24K Processor Core Family Software User's Manual
 *
 * 6.2.9 Count Register (CP0 Register 9, Select 0)
 * The Count register acts as a timer, incrementing at a constant
 * rate, whether or not an instruction is executed, retired, or
 * any forward progress is made through the pipeline.  The counter
 * increments every other clock, if the DC bit in the Cause register
 * is 0.
 */
/* Since the count is incremented every other tick, divide by 2 */
/* XXX derive this from CFG_PLL_FREQ */
#define CPU_PLL_DITHER_FRAC_VAL 0x001003e8
#define CPU_CLK_CONTROL_VAL1 0x00018004
#define CPU_CLK_CONTROL_VAL2 0x00008000

#if (CFG_PLL_FREQ == CFG_PLL_200_200_100)
#	define CFG_HZ          (200000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x41015000
        #define CPU_PLL_CONFIG_VAL2 0x01015000
    #else
        #define CPU_PLL_CONFIG_VAL1 0x41018000
        #define CPU_PLL_CONFIG_VAL2 0x01018000
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_200_200_200)
#	define CFG_HZ          (200000000/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00000004
    #define CPU_CLK_CONTROL_VAL2 0x00000000
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x41015000
        #define CPU_PLL_CONFIG_VAL2 0x01015000
    #else
        #define CPU_PLL_CONFIG_VAL1 0x41018000
        #define CPU_PLL_CONFIG_VAL2 0x01018000
    #endif    
#elif (CFG_PLL_FREQ == CFG_PLL_187_187_93)
#	define CFG_HZ          (187500000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001C03E8
        #define CPU_PLL_CONFIG_VAL1 0x41014B00
        #define CPU_PLL_CONFIG_VAL2 0x01014B00
    #else
        #define CPU_PLL_CONFIG_VAL1 0x41017800
        #define CPU_PLL_CONFIG_VAL2 0x01017800
    #endif    
#elif (CFG_PLL_FREQ == CFG_PLL_212_212_106)
#	define CFG_HZ          (212500000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001A03E8
        #define CPU_PLL_CONFIG_VAL1 0x40A12800
        #define CPU_PLL_CONFIG_VAL2 0x00A12800
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40A14400
        #define CPU_PLL_CONFIG_VAL2 0x00A14400
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_225_225_112)
#	define CFG_HZ          (225000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001403E8
        #define CPU_PLL_CONFIG_VAL1 0x40A12D00
        #define CPU_PLL_CONFIG_VAL2 0x00A12D00
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40A14800
        #define CPU_PLL_CONFIG_VAL2 0x00A14800
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_250_250_125)
#	define CFG_HZ          (250000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001803E8
        #define CPU_PLL_CONFIG_VAL1 0x40A13200
        #define CPU_PLL_CONFIG_VAL2 0x00A13200
    #else
        #if (CPU_PLL_REFDIV == 2)
            #define CPU_PLL_CONFIG_VAL1 0x40A2A000
            #define CPU_PLL_CONFIG_VAL2 0x00A2A000
        #else
        #define CPU_PLL_CONFIG_VAL1 0x40A15000
        #define CPU_PLL_CONFIG_VAL2 0x00A15000
    #endif
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_262_262_131)
#	define CFG_HZ          (262500000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001203E8
        #define CPU_PLL_CONFIG_VAL1 0x40A13480
        #define CPU_PLL_CONFIG_VAL2 0x00A13480
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40A15400
        #define CPU_PLL_CONFIG_VAL2 0x00A15400
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_266_266_133)
#	define CFG_HZ          (266666666/2)
    #undef CPU_PLL_DITHER_FRAC_VAL
    #define CPU_PLL_DITHER_FRAC_VAL 0x001557E8
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x40A13555
        #define CPU_PLL_CONFIG_VAL2 0x00A13555
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40A15555
        #define CPU_PLL_CONFIG_VAL2 0x00A15555
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_275_275_137)
#	define CFG_HZ          (275000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001C03E8
        #define CPU_PLL_CONFIG_VAL1 0x40A13700
        #define CPU_PLL_CONFIG_VAL2 0x00A13700
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40A15800
        #define CPU_PLL_CONFIG_VAL2 0x00A15800
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_300_300_150)
#	define CFG_HZ          (300000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x40813C00
        #define CPU_PLL_CONFIG_VAL2 0x00813C00    
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40816000
        #define CPU_PLL_CONFIG_VAL2 0x00816000
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_340_340_170)
#	define CFG_HZ          (340000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x40814400
        #define CPU_PLL_CONFIG_VAL2 0x00814400
    #else
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001337E8
        #define CPU_PLL_CONFIG_VAL1 0x40816CCD
        #define CPU_PLL_CONFIG_VAL2 0x00816CCD
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_350_350_175)
#	define CFG_HZ          (350000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001803E8
        #define CPU_PLL_CONFIG_VAL1 0x40814600
        #define CPU_PLL_CONFIG_VAL2 0x00814600    
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40817000
        #define CPU_PLL_CONFIG_VAL2 0x00817000
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_350_175_175) /* SDRAM */
#	define CFG_HZ          (350000000/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00018404
    #define CPU_CLK_CONTROL_VAL2 0x00008400
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001803E8
        #define CPU_PLL_CONFIG_VAL1 0x40814600
        #define CPU_PLL_CONFIG_VAL2 0x00814600
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40817000
        #define CPU_PLL_CONFIG_VAL2 0x00817000
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_333_333_166)
#	define CFG_HZ          (333333334/2)
    #undef CPU_PLL_DITHER_FRAC_VAL
    #define CPU_PLL_DITHER_FRAC_VAL 0x001AAFE8
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x408142AB
        #define CPU_PLL_CONFIG_VAL2 0x008142AB
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40816AAB
        #define CPU_PLL_CONFIG_VAL2 0x00816AAB
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_333_166_166) /* SDRAM */
#	define CFG_HZ          (333333334/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00018404
    #define CPU_CLK_CONTROL_VAL2 0x00008400
    #undef CPU_PLL_DITHER_FRAC_VAL
    #define CPU_PLL_DITHER_FRAC_VAL 0x001AAFE8
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x408142AB
        #define CPU_PLL_CONFIG_VAL2 0x008142AB
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40816AAB
        #define CPU_PLL_CONFIG_VAL2 0x00816AAB
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_325_162_162) /* SDRAM */
#	define CFG_HZ          (325000000/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00018404
    #define CPU_CLK_CONTROL_VAL2 0x00008400
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001403E8
        #define CPU_PLL_CONFIG_VAL1 0x40814100
        #define CPU_PLL_CONFIG_VAL2 0x00814100
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40816800
        #define CPU_PLL_CONFIG_VAL2 0x00816800
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_337_168_168) /* SDRAM */
#	define CFG_HZ          (337500000/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00018404
    #define CPU_CLK_CONTROL_VAL2 0x00008400
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001E03E8
        #define CPU_PLL_CONFIG_VAL1 0x40814380
        #define CPU_PLL_CONFIG_VAL2 0x00814380
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40816C00
        #define CPU_PLL_CONFIG_VAL2 0x00816C00
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_360_180_180) /* SDRAM */
#	define CFG_HZ          (360000000/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00018404
    #define CPU_CLK_CONTROL_VAL2 0x00008400
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x40814800
        #define CPU_PLL_CONFIG_VAL2 0x00814800    
    #else
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001CCFE8
        #define CPU_PLL_CONFIG_VAL1 0x40817333
        #define CPU_PLL_CONFIG_VAL2 0x00817333
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_380_190_190) /* SDRAM */
#	define CFG_HZ          (380000000/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00018404
    #define CPU_CLK_CONTROL_VAL2 0x00008400
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x40814C00
        #define CPU_PLL_CONFIG_VAL2 0x00814C00
    #else
        #undef CPU_PLL_DITHER_FRAC_VAL
        #define CPU_PLL_DITHER_FRAC_VAL 0x001667E8
        #define CPU_PLL_CONFIG_VAL1 0x40817999
        #define CPU_PLL_CONFIG_VAL2 0x00817999
    #endif 
#elif (CFG_PLL_FREQ == CFG_PLL_266_266_66)
#	define CFG_HZ          (266000000/2)
#elif (CFG_PLL_FREQ == CFG_PLL_400_400_200)
#	define CFG_HZ          (400000000/2)
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x40815000
        #define CPU_PLL_CONFIG_VAL2 0x00815000    
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40818000
        #define CPU_PLL_CONFIG_VAL2 0x00818000
    #endif
#elif (CFG_PLL_FREQ == CFG_PLL_400_200_200) /* SDRAM */
#	define CFG_HZ          (400000000/2)
    #undef CPU_CLK_CONTROL_VAL1
    #undef CPU_CLK_CONTROL_VAL2
    #define CPU_CLK_CONTROL_VAL1 0x00018404
    #define CPU_CLK_CONTROL_VAL2 0x00008400
    #if CONFIG_40MHZ_XTAL_SUPPORT
        #define CPU_PLL_CONFIG_VAL1 0x40815000
        #define CPU_PLL_CONFIG_VAL2 0x00815000
    #else
        #define CPU_PLL_CONFIG_VAL1 0x40818000
        #define CPU_PLL_CONFIG_VAL2 0x00818000
    #endif    
#elif (CFG_PLL_FREQ == CFG_PLL_320_320_80) || (CFG_PLL_FREQ == CFG_PLL_320_320_160)
#	define CFG_HZ          (320000000/2)
#elif (CFG_PLL_FREQ == CFG_PLL_410_400_200)
#	define CFG_HZ          (410000000/2)
#elif (CFG_PLL_FREQ == CFG_PLL_420_400_200)
#	define CFG_HZ          (420000000/2)
#elif (CFG_PLL_FREQ == CFG_PLL_362_362_181)
#	define CFG_HZ          (326500000/2)
    #define CPU_PLL_CONFIG_VAL1 0x40817400
    #define CPU_PLL_CONFIG_VAL2 0x00817400
#elif (CFG_PLL_FREQ == CFG_PLL_80_80_40)
#	define CFG_HZ          (80000000/2)
#elif (CFG_PLL_FREQ == CFG_PLL_64_64_32)
#	define CFG_HZ          (64000000/2)
#elif (CFG_PLL_FREQ == CFG_PLL_48_48_24)
#	define CFG_HZ          (48000000/2)
#elif (CFG_PLL_FREQ == CFG_PLL_32_32_16)
#	define CFG_HZ          (32000000/2)
#endif

#if CONFIG_40MHZ_XTAL_SUPPORT
    #define CPU_PLL_SETTLE_TIME_VAL    0x00000550
#else
    #define CPU_PLL_SETTLE_TIME_VAL    0x00000352
#endif

/* 
 * timeout values are in ticks 
 */
#define CFG_FLASH_ERASE_TOUT	(2 * CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT	(2 * CFG_HZ) /* Timeout for Flash Write */

/*
 * Cache lock for stack
 */
#define CFG_INIT_SP_OFFSET	0x1000

#ifndef COMPRESSED_UBOOT
#define	CFG_ENV_IS_IN_FLASH    1
#undef CFG_ENV_IS_NOWHERE  
#else
#undef  CFG_ENV_IS_IN_FLASH
#define CFG_ENV_IS_NOWHERE  1
#endif /* #ifndef COMPRESSED_UBOOT */

/* Address and size of Primary Environment Sector	*/
#define CFG_ENV_ADDR		0x9f040000
#define CFG_ENV_SIZE		0x10000

#if (FLASH_SIZE == 4)
    #define CONFIG_BOOTCOMMAND "bootm 0x9f300000"
#else
    #ifdef VXWORKS_UBOOT
       #define CONFIG_BOOTCOMMAND "bootm 0x9f050000"
    #else
       #define CONFIG_BOOTCOMMAND "bootm 0x9f140000"
    #endif
#endif /* #ifndef COMPRESSED_UBOOT */

//#define CONFIG_FLASH_16BIT

/* DDR init values */

#define CONFIG_NR_DRAM_BANKS	2
#if CONFIG_40MHZ_XTAL_SUPPORT
#define CFG_SDR_REFRESH_VAL     0x4270
#define CFG_DDR_REFRESH_VAL     0x4138
#else
#define CFG_SDR_REFRESH_VAL     0x4186
#define CFG_DDR_REFRESH_VAL     0x40c3
#endif

#if (DDR_CAS_LATENCY_VAL == 3)
#define CFG_DDR_CONFIG_VAL      0x7fbc8cd0
    //#define CFG_DDR_CONFIG2_VAL	    0x99d0e6a8      // HORNET 1.0
    #define CFG_DDR_CONFIG2_VAL	    0x9dd0e6a8      // HORNET 1.1
#define CFG_DDR_MODE_VAL_INIT   0x133
    #define CFG_DDR_MODE_VAL        0x33
#elif (DDR_CAS_LATENCY_VAL == 2)
    #define CFG_DDR_CONFIG_VAL      0x67bc8cd0
    #define CFG_DDR_CONFIG2_VAL	    0x91d0e6a8
    #define CFG_DDR_MODE_VAL_INIT   0x123
    #define CFG_DDR_MODE_VAL        0x23
#else /* CAS Latency 2.5 */
    #define CFG_DDR_CONFIG_VAL      0x6fbc8cd0
    #define CFG_DDR_CONFIG2_VAL	    0x95d0e6a8
    #define CFG_DDR_MODE_VAL_INIT   0x163
    #define CFG_DDR_MODE_VAL        0x63
#endif

#ifdef LOW_DRIVE_STRENGTH
#	define CFG_DDR_EXT_MODE_VAL    0x2
#else
#	define CFG_DDR_EXT_MODE_VAL    0x0
#endif

#define CFG_DDR_TRTW_VAL        0x1f
#define CFG_DDR_TWTR_VAL        0x1e

#define CFG_DDR_RD_DATA_THIS_CYCLE_VAL  0x00ff

#define CFG_DDR_CONFIG_VAL_SDRAM            0x5fbe8cd0
#define CFG_DDR_CONFIG2_VAL_SDRAM           0x919f66a8
#define CFG_DDR_CONFIG_VAL_SDRAM_TB454      0x67be8cd0
#define CFG_DDR_CONFIG2_VAL_SDRAM_TB454     0x959f66a8

#ifndef CONFIG_HORNET_EMU
#define CFG_DDR_TAP0_VAL        0x8
#define CFG_DDR_TAP1_VAL        0x9
#else
#define CFG_DDR_TAP0_VAL        0x8
#define CFG_DDR_TAP1_VAL        0x9
#endif

/* DDR2 Init values */
#define CFG_DDR2_EXT_MODE_VAL    0x402

/* DDR value from Flash */
#ifdef ENABLE_DYNAMIC_CONF
#define CFG_DDR_MAGIC           0xaabacada
#define CFG_DDR_MAGIC_F         (UBOOT_ENV_SEC_START + CFG_FLASH_SECTOR_SIZE - 0x30)
#define CFG_DDR_CONFIG_VAL_F    *(volatile int *)(CFG_DDR_MAGIC_F + 4)
#define CFG_DDR_CONFIG2_VAL_F	*(volatile int *)(CFG_DDR_MAGIC_F + 8)
#define CFG_DDR_EXT_MODE_VAL_F  *(volatile int *)(CFG_DDR_MAGIC_F + 12)
#endif

#define CONFIG_NET_MULTI

#define CONFIG_MEMSIZE_IN_BYTES

#ifndef COMPRESSED_UBOOT
#	ifndef CONFIG_MACH_HORNET
#		define CONFIG_PCI 1
#	endif
#else
#	define ATH_NO_PCI_INIT
#endif /* #ifndef COMPRESSED_UBOOT */

/*-----------------------------------------------------------------------
 * Cache Configuration
 */
#ifndef COMPRESSED_UBOOT
#define CONFIG_COMMANDS	(( CONFIG_CMD_DFL | CFG_CMD_DHCP | CFG_CMD_ELF | CFG_CMD_PCI |	\
	CFG_CMD_MII | CFG_CMD_PING | CFG_CMD_NET | CFG_CMD_ENV |	\
	CFG_CMD_FLASH | CFG_CMD_LOADS | CFG_CMD_RUN | CFG_CMD_LOADB | CFG_CMD_ELF | CFG_CMD_ETHREG ))
#elif defined(VXWORKS_UBOOT)
#define CONFIG_COMMANDS	(( CONFIG_CMD_DFL | CFG_CMD_PING | CFG_CMD_NET | CFG_CMD_MII | CFG_CMD_ELF))
#else
#define CONFIG_COMMANDS	(( CONFIG_CMD_DFL | CFG_CMD_PING | CFG_CMD_NET | CFG_CMD_MII))
#endif /* #ifndef COMPRESSED_UBOOT */

#define CFG_ATHRS26_PHY  1

#define CONFIG_IPADDR   192.168.1.2
#define CONFIG_SERVERIP 192.168.1.10
#define CONFIG_ETHADDR 0x00:0xaa:0xbb:0xcc:0xdd:0xee
#define CFG_FAULT_ECHO_LINK_DOWN    1


#define CFG_PHY_ADDR 0 

#ifdef CONFIG_HORNET_EMU
#define CFG_AG7240_NMACS 1
#else
#define CFG_AG7240_NMACS 2
#endif
#define CFG_GMII     0
#define CFG_MII0_RMII             1
#define CFG_AG7100_GE0_RMII             1

#define CFG_BOOTM_LEN	(16 << 20) /* 16 MB */
#define DEBUG
#define CFG_HUSH_PARSER
#define CFG_PROMPT_HUSH_PS2 "hush>"

/*
** Parameters defining the location of the calibration/initialization
** information for the two Merlin devices.
** NOTE: **This will change with different flash configurations**
*/

#if (FLASH_SIZE == 4)
#define WLANCAL                        0xbf3f1000
#define BOARDCAL                       0xbf3f0000
#else
#define WLANCAL                        0xbf1f1000
#define BOARDCAL                       0xbf1f0000
#endif

#define ATHEROS_PRODUCT_ID             138
#define CAL_SECTOR                     (CFG_MAX_FLASH_SECT - 1)

/* For Kite, only PCI-e interface is valid */
#define AR7240_ART_PCICFG_OFFSET        3



#include <cmd_confdefs.h>

#endif	/* __CONFIG_H */

