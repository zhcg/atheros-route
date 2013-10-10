#include <asm/addrspace.h>
#include <wasp_api.h>
#include <gmac_api.h>
#include <ar7240_soc.h>
#include <phy.h>

unsigned int
athrs27_reg_read(unsigned int s27_addr)
{
	unsigned int	addr_temp, s27_rd_csr_low, s27_rd_csr_high,
			s27_rd_csr, data, unit, phy_address, reg_address;

	addr_temp = s27_addr >> 2;
	data = addr_temp >> 7;

	phy_address = 0x1f;
	reg_address = 0x10;

	unit = 1;

	phy_reg_write(unit, phy_address, reg_address, data);

	phy_address = (0x17 & ((addr_temp >> 4) | 0x10));
	reg_address = ((addr_temp << 1) & 0x1e);
	s27_rd_csr_low = (uint32_t) phy_reg_read(unit, phy_address, reg_address);

	reg_address = reg_address | 0x1;
	s27_rd_csr_high = (uint32_t) phy_reg_read(unit, phy_address, reg_address);
	s27_rd_csr = (s27_rd_csr_high << 16) | s27_rd_csr_low;

	return s27_rd_csr;
}

void
athrs27_reg_write(unsigned int s27_addr, unsigned int s27_write_data)
{
	unsigned int addr_temp, data, phy_address, reg_address, unit;

	addr_temp = (s27_addr ) >> 2;
	data = addr_temp >> 7;

	phy_address = 0x1f;
	reg_address = 0x10;

	unit = 1;

	phy_reg_write(unit, phy_address, reg_address, data);

	phy_address = (0x17 & ((addr_temp >> 4) | 0x10));

	reg_address = (((addr_temp << 1) & 0x1e) | 0x1);
	data = (s27_write_data >> 16) & 0xffff;
	phy_reg_write(unit, phy_address, reg_address, data);

	reg_address = ((addr_temp << 1) & 0x1e);
	data = s27_write_data  & 0xffff;
	phy_reg_write(unit, phy_address, reg_address, data);
}

void
athrs27_reg_rmw(unsigned int s27_addr, unsigned int s27_write_data)
{
	int val = athrs27_reg_read(s27_addr);
	athrs27_reg_write(s27_addr, (val | s27_write_data));
}

unsigned int
s27_rd_phy(int ethUnit, unsigned int phy_addr, unsigned int reg_addr)
{

	unsigned int rddata, i = 100;

	if (phy_addr >= ATHR_PHY_MAX) {
		return -1 ;
	}

	/* MDIO_CMD is set for read */
	rddata = athrs27_reg_read(0x98);
	rddata = (rddata & 0x0) | (reg_addr << 16) |
			(phy_addr << 21) | (1 << 27) |
			(1 << 30) | (1 << 31);

	athrs27_reg_write(0x98, rddata);

	/* Check MDIO_BUSY status */
	do {
		rddata = athrs27_reg_read(0x98);
		rddata = rddata & (1 << 31);
	} while (rddata && --i);

	/* Read the data from phy */
	rddata = athrs27_reg_read(0x98);
	rddata = rddata & 0xffff;

	return rddata;
}

void
s27_wr_phy(int ethUnit, unsigned int phy_addr, unsigned int reg_addr,
		unsigned int write_data)
{
	unsigned int rddata, i = 100;

	if (phy_addr >= ATHR_PHY_MAX) {
		return ;
	}

	/* MDIO_CMD is set for read */
	rddata = athrs27_reg_read(0x98);
	rddata = (rddata & 0x0) | (write_data & 0xffff) |
			(reg_addr << 16) | (phy_addr << 21) |
			(0 << 27) | (1 << 30) | (1 << 31);

	athrs27_reg_write(0x98, rddata);

	/* Check MDIO_BUSY status */
	do {
		rddata = athrs27_reg_read(0x98);
		rddata = rddata & (1 << 31);
	} while (rddata && --i);
}

uint16_t
athr_gmac_mii_read(int unit, uint32_t phy_addr, uint8_t reg)
{
	uint16_t	addr = (phy_addr << ATHR_GMAC_ADDR_SHIFT) | reg, val;
	volatile int	rddata;
	uint16_t	ii = 0x1000;

	athr_gmac_reg_wr(unit, ATHR_GMAC_MII_MGMT_CMD, 0x0);
	athr_gmac_reg_wr(unit, ATHR_GMAC_MII_MGMT_ADDRESS, addr);
	athr_gmac_reg_wr(unit, ATHR_GMAC_MII_MGMT_CMD, ATHR_GMAC_MGMT_CMD_READ);

	do
	{
		udelay(5);
		rddata = athr_gmac_reg_rd(unit, ATHR_GMAC_MII_MGMT_IND) & 0x1;
	} while (rddata && --ii);

	val = athr_gmac_reg_rd(unit, ATHR_GMAC_MII_MGMT_STATUS);
	athr_gmac_reg_wr(unit, ATHR_GMAC_MII_MGMT_CMD, 0x0);

	return val;
}

void
athr_gmac_mii_write(int unit, uint32_t phy_addr, uint8_t reg, uint16_t data)
{
	uint16_t      addr  = (phy_addr << ATHR_GMAC_ADDR_SHIFT) | reg;
	volatile int rddata;
	uint16_t      ii = 0x1000;

	athr_gmac_reg_wr(unit, ATHR_GMAC_MII_MGMT_ADDRESS, addr);
	athr_gmac_reg_wr(unit, ATHR_GMAC_MII_MGMT_CTRL, data);

	do
	{
		rddata = athr_gmac_reg_rd(unit, ATHR_GMAC_MII_MGMT_IND) & 0x1;
	} while(rddata && --ii);
}
