#ifndef _ATHR_PHY_H
#define _ATHR_PHY_H

#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <asm/system.h>
#include <linux/netdevice.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31))
#include <linux/autoconf.h>
#else
#include <linux/config.h>
#endif


#if defined(CONFIG_MACH_AR7100)
#include "ag7100.h"
#include "ar7100.h"

#define is_ar7100() 1
#endif


#ifndef is_ar7240
#define is_ar7240() 0
#endif

#if defined(CONFIG_AR7240_S26_PHY) || defined(CONFIG_ATHRS26_PHY)
#include "ar7240_s26_phy.h"
#undef is_s26
#define is_s26() 1
#endif

#ifdef CONFIG_ATHRS27_PHY
#include "athrs27_phy.h"
#undef is_s27
#define is_s27() 1
#endif

#if defined(CONFIG_AR7242_RGMII_PHY) || defined(CONFIG_ATHRF1_PHY)
#undef is_f1e
#define is_f1e() 1
#endif

#if defined(CONFIG_ATHRS16_PHY) || defined(CONFIG_AR7242_S16_PHY)
#include "athrs16_phy.h"
#undef is_s16
#define is_s16() 1
#endif

#ifdef CONFIG_AR8021_PHY
#include "ar8021_phy.h"
#undef is_8021
#define is_8021() 1
#endif

#ifdef CONFIG_ATHRS_VIR_PHY
#include "athrs_vir_phy.h"
#undef is_vir_phy
#define is_vir_phy() 1
#endif


/*****************/
/* PHY Registers */
/*****************/
#define ATHR_PHY_CONTROL                 0
#define ATHR_PHY_STATUS                  1
#define ATHR_PHY_ID1                     2
#define ATHR_PHY_ID2                     3
#define ATHR_AUTONEG_ADVERT              4
#define ATHR_LINK_PARTNER_ABILITY        5
#define ATHR_AUTONEG_EXPANSION           6
#define ATHR_NEXT_PAGE_TRANSMIT          7
#define ATHR_LINK_PARTNER_NEXT_PAGE      8
#define ATHR_1000BASET_CONTROL           9
#define ATHR_1000BASET_STATUS            10
#define ATHR_PHY_SPEC_CONTROL            16
#define ATHR_PHY_SPEC_STATUS             17
#define ATHR_INTR_STATUS                 19
#define ATHR_EXT_PHY_SPEC_CONTROL        20

/* ATHR_PHY_CONTROL fields */
#define ATHR_CTRL_SOFTWARE_RESET                    0x8000
#define ATHR_CTRL_SPEED_LSB                         0x2000
#define ATHR_CTRL_AUTONEGOTIATION_ENABLE            0x1000
#define ATHR_CTRL_RESTART_AUTONEGOTIATION           0x0200
#define ATHR_CTRL_SPEED_FULL_DUPLEX                 0x0100
#define ATHR_CTRL_SPEED_MSB                         0x0040

#define ATHR_RESET_DONE(phy_control)                   \
    (((phy_control) & (ATHR_CTRL_SOFTWARE_RESET)) == 0)
    
/* Phy status fields */
#define ATHR_STATUS_AUTO_NEG_DONE                   0x0020
#define ATHR_STATUS_LINK_STATUS                     0x0004

#define ATHR_AUTONEG_DONE(ip_phy_status)                   \
    (((ip_phy_status) &                                  \
        (ATHR_STATUS_AUTO_NEG_DONE)) ==                    \
        (ATHR_STATUS_AUTO_NEG_DONE))

/* Link Partner ability */
#define ATHR_LINK_100BASETX_FULL_DUPLEX       0x0100
#define ATHR_LINK_100BASETX                   0x0080
#define ATHR_LINK_10BASETX_FULL_DUPLEX        0x0040
#define ATHR_LINK_10BASETX                    0x0020

/* Advertisement register. */
#define ATHR_ADVERTISE_NEXT_PAGE              0x8000
#define ATHR_ADVERTISE_ASYM_PAUSE             0x0800
#define ATHR_ADVERTISE_PAUSE                  0x0400
#define ATHR_ADVERTISE_100FULL                0x0100
#define ATHR_ADVERTISE_100HALF                0x0080  
#define ATHR_ADVERTISE_10FULL                 0x0040  
#define ATHR_ADVERTISE_10HALF                 0x0020  

#define ATHR_ADVERTISE_ALL (ATHR_ADVERTISE_10HALF | ATHR_ADVERTISE_10FULL | \
                            ATHR_ADVERTISE_100HALF | ATHR_ADVERTISE_100FULL)
                       
/* 1000BASET_CONTROL */
#define ATHR_ADVERTISE_1000FULL               0x0200
#define ATHR_ADVERTISE_1000HALF               0x0100

/* Phy Specific status fields */
#define ATHR_STATUS_SPEED_MASK                0xC000
#define ATHR_STATUS_SPEED_SHIFT               14
#define ATHR_STATUS_FULL_DUPLEX               0x2000
#define ATHR_STATUS_RESOLVED                  0x0800
#define ATHR_STATUS_LINK_UP                   0x0400 
#define ATHR_STATUS_SMARTSPEED_DOWN           0x0020

int athr_phy_is_up(int unit);
int athr_phy_is_fdx(int unit);
int athr_phy_speed(int unit);
int athr_phy_setup(int unit);

#endif /* _ATHR_PHY_H */

