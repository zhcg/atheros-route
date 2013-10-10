/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright © 2003 Atheros Communications, Inc.,  All Rights Reserved.
 */

/*
 * Manage the atheros Remote PHY.
 *
 * All definitions in this file are operating system independent!
 */

//#define printk             DEBUG_PRINTF
#define udelay             A_UDELAY
#define mdelay(_x)         udelay((_x)*1000)
#include <linux/kernel.h>
#include <asm/delay.h>
#include "ar7100.h"

#ifndef VERBOSE
#define  VERBOSE           0
#endif

#include "ag7100.h"
#include "ag7100_phy.h"

#define ATHR_STATUS_LINK_PASS 0x0400
#define ATHER_STATUS_FULL_DEPLEX 0x2000

#define MODULE_NAME "ATHRRPHY"

typedef struct {
  int              is_enet_port;
  int              mac_unit;
  unsigned int     phy_addr;
}athr_phy_t;

athr_phy_t phy_info1[] = {
    {is_enet_port: 1,
     mac_unit    : 0,
     phy_addr    : 0x01}
};

static athr_phy_t *
athr_vir_phy_find(int unit)
{
    int i;
    athr_phy_t *phy;

    for(i = 0; i < sizeof(phy_info1)/sizeof(athr_phy_t); i++) {
        phy = &phy_info1[i];
        
        if (phy->is_enet_port && (phy->mac_unit == unit)) 
            return phy;
    }
    
    return NULL;
}

int
athr_vir_phy_setup(int unit)
{
    athr_phy_t *phy = athr_vir_phy_find(unit);
    uint16_t  phyHwStatus;
    uint16_t  timeout;

    if (!phy) {
        printk(MODULE_NAME": \nNo phy found for unit %d\n", unit);
        return;
    }
    printk(MODULE_NAME": unit %d phy addr %x ", unit, phy->phy_addr);
}

int
athr_vir_phy_is_up(int unit)
{
    int status;
    athr_phy_t *phy = athr_vir_phy_find(unit);

    if (!phy) 
        return 0;

    status = ATHR_STATUS_LINK_PASS;
    if (status & ATHR_STATUS_LINK_PASS)
        return 1;

    return 0;
}

int
athr_vir_phy_is_fdx(int unit)
{
    int status;
    athr_phy_t *phy = athr_vir_phy_find(unit);
    int ii = 200;

    if (!phy) 
        return 0;
    status = ATHER_STATUS_FULL_DEPLEX;
    status = !(!(status & ATHER_STATUS_FULL_DEPLEX));
    return (status);
}

int
athr_vir_phy_speed(int unit)
{
    int status;
    athr_phy_t *phy = athr_vir_phy_find(unit);
    int ii = 200;

    if (!phy) 
        return 0;
    status = AG7100_PHY_SPEED_1000T;
    switch(status) {
    case 0:
        return AG7100_PHY_SPEED_10T;
    case 1:
        return AG7100_PHY_SPEED_100TX;
    case 2:
        return AG7100_PHY_SPEED_1000T;
    default:
        printk(MODULE_NAME": Unkown speed read!\n");
    }
    return -1;
}

int
athr_vir_phy_status(int unit, int *link, int *fdx, int *speed)
{
    athr_phy_t *phy = athr_vir_phy_find(unit);
    //printk("VIR PHY STATUS\n");
    if (!phy) {
        *link = 0;
        return 0;
    }   

    *link = 1;
    *fdx = 1;
    *speed = AG7100_PHY_SPEED_1000T;
    return 0;
}

