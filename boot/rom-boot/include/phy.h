#ifndef __PHY_H_
#define __PHY_H_

#define PORT_STATUS_REGISTER0		0x0100
#define PORT_STATUS_REGISTER1		0x0200
#define PORT_STATUS_REGISTER2		0x0300
#define PORT_STATUS_REGISTER3		0x0400
#define PORT_STATUS_REGISTER4		0x0500
#define PORT_STATUS_REGISTER5		0x0600

#define PORT_CONTROL_REGISTER0		0x0104
#define PORT_CONTROL_REGISTER5		0x0604


#define OPERATIONAL_MODE_REG0		0x4

#define ATHR_PHY_MAX			4
#define ENET_UNIT_WAN			0
#define ATHR_PHY_CONTROL		0
#define ATHR_PHY_ID1			2
#define ATHR_PHY_SPEC_STATUS		17
#define ATHR_PHY_STATUS			1
#define ETH_SWONLY_MODE			0x0008
#define ATHR_PHY4_ADDR			0x4


#define ATHR_DEBUG_PORT_ADDRESS		29
#define ATHR_DEBUG_PORT_DATA		30

#define ATHR_PHY_FUNC_CONTROL		16

#define S27_ARL_TBL_CTRL_REG		0x005c
#define S27_FLD_MASK_REG		0x002c
#define S27_ENABLE_CPU_BROADCAST	(1 << 26)
#define S27_ENABLE_CPU_BCAST_FWD	(1 << 25)

/*
 * MII registers
 */
#define ATHR_GMAC_MII_MGMT_CFG		0x20
#define ATHR_GMAC_MGMT_CFG_CLK_DIV_20	0x06
#define ATHR_GMAC_MII_MGMT_CMD		0x24
#define ATHR_GMAC_MGMT_CMD_READ		0x1
#define ATHR_GMAC_MII_MGMT_ADDRESS	0x28
#define ATHR_GMAC_ADDR_SHIFT		8
#define ATHR_GMAC_MII_MGMT_CTRL		0x2c
#define ATHR_GMAC_MII_MGMT_STATUS	0x30
#define ATHR_GMAC_MII_MGMT_IND		0x34
#define ATHR_GMAC_MGMT_IND_BUSY		(1 << 0)
#define ATHR_GMAC_MGMT_IND_INVALID	(1 << 2)
#define ATHR_GMAC_GE_MAC_ADDR1		0x40
#define ATHR_GMAC_GE_MAC_ADDR2		0x44
#define ATHR_GMAC_MII0_CONTROL		0x18070000


unsigned int athrs27_reg_read(unsigned int);
void athrs27_reg_write(unsigned int, unsigned int);
void athrs27_reg_rmw(unsigned int, unsigned int);
unsigned int s27_rd_phy(int, unsigned int, unsigned int);
void s27_wr_phy(int, unsigned int, unsigned int, unsigned int);

uint16_t athr_gmac_mii_read(int, uint32_t, uint8_t);
void athr_gmac_mii_write(int, uint32_t, uint8_t, uint16_t);

#define phy_reg_read			athr_gmac_mii_read
#define phy_reg_write			athr_gmac_mii_write

#endif /* __PHY_H_ */
