/*
 * This file contains glue for Atheros ath spi flash interface
 * Primitives are ath_spi_*
 * mtd flash implements are ath_flash_*
 */
#include <linux/init.h>
#include <linux/config.h>
#include <linux/fs.h>    
#include <linux/types.h>  
#include <linux/fcntl.h>   
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <asm/system.h>   


#include <atheros.h>
#include "ath_spi_si3217x.h"
#ifdef  CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#define B6_V3
//#ifdef B6_V3
//#undef B6_V3
//#endif

#define CNUM_TO_CID_QUAD(channelNumber)   (((channelNumber<<4)&0x10)|((channelNumber<<2)&0x8)|((channelNumber>>2)&0x2)|((channelNumber>>4)&0x1)|(channelNumber&0x4))

int spidrv_major =  217;
//int spich = 0;
void WriteReg (unsigned short portID, unsigned char channel, unsigned char regAddr, unsigned char data);
unsigned char ReadReg (unsigned short portID,  unsigned char channel,  unsigned char regAddr);
void WriteRam (unsigned short portID, unsigned char channel, unsigned short regAddr, unsigned int data);
unsigned int ReadRam (unsigned short portID, unsigned char channel, unsigned short regAddr);
int ath_spi_si3217x_reset(int status);


extern void ath_flash_spi_down(void);
extern void ath_flash_spi_up(void);

#if 1
void si_spi_test(void)
{
	int i;
	unsigned char regData;
	printk("SI :: Reset ......\n");
	ath_spi_si3217x_reset(1);
	mdelay(1000);
	ath_spi_si3217x_reset(0);
//	for(i = 0; i < 10; i++){
		regData = ReadReg(0, 0x0, 0);
		printk("SI :: ID Register: 0x%x \n", regData);
		printk("SI :: Part Number: 0x%x \n", (regData & 0x38) >> 3);
		printk("SI :: Revision Identification: 0x%x \n", (regData & 0x07));
//	}

//	WriteReg(0, 0, 66, 0x55);
	regData = 0;
	regData = ReadReg(0, 0, 74);
	printk("SI :: FXO_CNTL Register: 0x%x \n", regData);
	WriteReg(0, 0, 74, regData|0x01);

	for(i = 0; i < 60; i++){
		regData = ReadReg(0, 0x1, i);
		printk("SI :: FXO %d Register: 0x%x \n", i, regData);
	}


	
}
#endif

int spidrv_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int spidrv_release(struct inode *inode, struct file *filp)
{
	return 0;
}

int spidrv_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	unsigned char regAddr, regData;
	unsigned short ramAddr, ramData;
	int sid;
	unsigned char cid;
	SI3217x_REG_DATA *reg_data;
	SI3217x_RAM_DATA *ram_data;

	switch (cmd) {
	case SPI_SI_REG_READ:
		reg_data = (SI3217x_REG_DATA*)arg;
		sid = reg_data->sid;
		cid = reg_data->channel;
		regAddr = reg_data->regAddr;
		regData = ReadReg(sid, cid, regAddr);
		reg_data->data = regData;
//		printk("SPI_SI_REG_READ :: 0x%02x : 0x%02x\n", regAddr, regData);
		break;
	case SPI_SI_REG_WRITE:
		reg_data = (SI3217x_REG_DATA*)arg;
		sid = reg_data->sid;
		cid = reg_data->channel;
		regAddr = reg_data->regAddr;
		regData = reg_data->data;
		WriteReg(sid, cid, regAddr, regData);
//		printk("SPI_SI_REG_WRITE :: 0x%02x : 0x%02x\n", regAddr, regData);
		break;
	case SPI_SI_RAM_READ:
		ram_data = (SI3217x_RAM_DATA*)arg;
		sid = ram_data->sid;
		cid = ram_data->channel;
		ramAddr = ram_data->ramAddr;
		ramData = ReadRam(sid, cid, ramAddr);
		ram_data->data = ramData;
//		printk("SPI_SI_RAM_READ :: 0x%04x : 0x%08x\n", ramAddr, ramData);
		break;
	case SPI_SI_RAM_WRITE:
		ram_data = (SI3217x_RAM_DATA*)arg;
		sid = ram_data->sid;
		cid = ram_data->channel;
		ramAddr = ram_data->ramAddr;
		ramData = ram_data->data;
		WriteRam(sid, cid, ramAddr, ramData);
//		printk("SPI_SI_RAM_WRITE :: 0x%04x : 0x%08x\n", ramAddr, ramData);
		break;
	case SPI_SI_RESET:
		ath_spi_si3217x_reset(arg);
//		printk("SPI_SI_RESET :: \n");
		break;
	default:
//		si_spi_test();
		printk("ath_spi_si3217x: command format error\n");
	}

	return 0;
}

struct file_operations spidrv_fops = {
    ioctl:      spidrv_ioctl,
    open:       spidrv_open,
    release:    spidrv_release,
};

int ath_spi_si3217x_reset(int status)
{
	unsigned int rddata;

	ath_reg_rmw_clear(ATH_GPIO_OE, 1<<15);

	rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION3);
	rddata = rddata & 0xffffff;
	rddata = rddata | ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_15(0x00);
	ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);

	if (status) {
		ath_reg_rmw_clear(ATH_GPIO_OUT, 1<<15);
	} else  {
		ath_reg_rmw_set(ATH_GPIO_OUT, 1<<15);
	}
	return 0;
}


int ath_spi_si3217x_gpio_init(void)
{
	unsigned int rddata;
#ifdef B6_V3
	//set gpio17 used as spi_cs for si32178 on B6_V3 board
    rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION4);
    rddata = rddata & 0xffff00ff;
    rddata = rddata | ((0x07) << 8);
    ath_reg_wr(ATH_GPIO_OUT_FUNCTION4, rddata);

	ath_reg_rmw_clear(ATH_GPIO_OE, 1<<17);
#else
	
	ath_reg_rmw_set(ATH_GPIO_FUNCTIONS,
			   ATH_GPIO_FUNCTION_JTAG_DISABLE);
	ath_reg_rmw_clear(ATH_GPIO_OE, 1<<0);

	rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION0);
	rddata = rddata & 0xffffff00;
	rddata = rddata | ATH_GPIO_OUT_FUNCTION0_ENABLE_GPIO_0(0x07);
	ath_reg_wr(ATH_GPIO_OUT_FUNCTION0, rddata);

//	rddata = ath_reg_rd(ATH_SPI_CLOCK);
//	rddata = rddata | 0x03;
//	ath_reg_wr(ATH_SPI_CLOCK, rddata);
#endif

	return 0;
}

static int __init ath_spi_si3217x_init(void)
{

	printk("ath_spi_si3217x_init\n");
	ath_spi_si3217x_gpio_init();

#ifdef  CONFIG_DEVFS_FS
	if(devfs_register_chrdev(spidrv_major, SPI_DEV_NAME , &spidrv_fops)) {
		printk(KERN_WARNING " spidrv: can't create device node\n");
		return -EIO;
		}
	
	devfs_handle = devfs_register(NULL, SPI_DEV_NAME, DEVFS_FL_DEFAULT, spidrv_major, 0,
					S_IFCHR | S_IRUGO | S_IWUGO, &spidrv_fops, NULL);
#else
	int result=0;
	result = register_chrdev(spidrv_major, SPI_DEV_NAME, &spidrv_fops);
	if (result < 0) {
			printk(KERN_WARNING "spi_drv: can't get major %d\n",spidrv_major);
			return result;
		}
	
	if (spidrv_major == 0) {
		spidrv_major = result; /* dynamic */
		}
#endif
//	ath_reg_wr(ATH_SPI_FS, 0);
	return 0;
}

static void __exit ath_spi_si3217x_exit(void)
{
	printk("ath_spi_si3217x_exit\n");
	
#ifdef  CONFIG_DEVFS_FS
	devfs_unregister_chrdev(spidrv_major, SPI_DEV_NAME);
	devfs_unregister(devfs_handle);
#else
	unregister_chrdev(spidrv_major, SPI_DEV_NAME);
#endif

}

static void ath_spi_enable(void)
{
	ath_reg_wr_nf(ATH_SPI_FS, 1);
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
	//ath_reg_rmw_set(ATH_GPIO_OUT, 1<<0);
	//printk("SI :: ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH) \n");
	//ndelay(200);
}
static void ath_spi_done(void)
{
	
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
	ath_reg_wr_nf(ATH_SPI_FS, 0);
	//ath_reg_rmw_set(ATH_GPIO_OUT, 1<<0);
	//printk("SI :: ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH) \n");
	
}
/*----------------------------------------------------------------------*/
/*   Function                                                           */
/*           	spi_chip_select                                         */
/*    INPUTS: ENABLE or DISABLE                                         */
/*   RETURNS: None                                                      */
/*   OUTPUTS: None                                                      */
/*   NOTE(S): Pull on or Pull down #CS                                  */
/*                                                                      */
/*----------------------------------------------------------------------*/

static void spi_chip_select(u8 enable)
{
	if (enable) {
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_ENABLE_1 | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_ENABLE_1 | ATH_SPI_CLK_HIGH);
		//ath_reg_rmw_clear(ATH_GPIO_OUT, 1<<0);
	} else  {
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		//ath_reg_rmw_set(ATH_GPIO_OUT, 1<<0);
	}		
}

/*----------------------------------------------------------------------*/
/*   Function                                                           */
/*           	spi_write                                               */
/*    INPUTS: 8-bit data                                                */
/*   RETURNS: None                                                      */
/*   OUTPUTS: None                                                      */
/*   NOTE(S): transfer 8-bit data to SPI                                */
/*                                                                      */
/*----------------------------------------------------------------------*/
static void spi_write(u8 data)
{
	ath_spi_bit_banger(data);
}

/*----------------------------------------------------------------------*/
/*   Function                                                           */
/*           	spi_read                                                */
/*    INPUTS: None                                                      */
/*   RETURNS: 8-bit data                                                */
/*   OUTPUTS: None                                                      */
/*   NOTE(S): get 8-bit data from SPI                                   */
/*                                                                      */
/*----------------------------------------------------------------------*/
static u8 spi_read(void) 
{
	ath_spi_delay_8();
	return (ath_reg_rd(ATH_SPI_RD_STATUS) & 0xff);
}


u8 spi_si3217x_read8(unsigned short sid, unsigned char cid, unsigned char reg)
{
	unsigned char value;
	unsigned char regCtrl = CNUM_TO_CID_QUAD(cid)|0x60;
	ath_flash_spi_down();
	ath_spi_enable();
	spi_chip_select(ENABLE);
	spi_write(regCtrl);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(reg);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	value = spi_read();
	spi_chip_select(DISABLE);	
	ath_spi_done();
	ath_flash_spi_up();	
	return value;
}

void spi_si3217x_write8(unsigned short sid, unsigned char cid, unsigned char reg, unsigned char value)
{ 
	unsigned char regCtrl = CNUM_TO_CID_QUAD(cid)|0x20;
	ath_flash_spi_down();
	ath_spi_enable();
	spi_chip_select(ENABLE);
	spi_write(regCtrl);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(reg);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(value);
	spi_chip_select(DISABLE);
	ath_spi_done();
	ath_flash_spi_up();
}
 unsigned char ReadReg (unsigned short portID,  unsigned char channel,  unsigned char regAddr)
{
	
	return spi_si3217x_read8(portID, channel, regAddr);
	
}

 void WriteReg (unsigned short portID, unsigned char channel, unsigned char regAddr, unsigned char data)
{	
	spi_si3217x_write8(portID, channel, regAddr, data);
}

void RAMwait (unsigned short portID,unsigned char channel)
{
	unsigned char regVal; 
	regVal = ReadReg (portID,channel,4);
	while (regVal&0x01)
	{
		regVal = ReadReg (portID,channel,4);
	}//wait for indirect registers

}
void WriteRam (unsigned short portID, unsigned char channel, unsigned short regAddr, unsigned int data)
{
 
	RAMwait(portID,channel);   
	WriteReg(portID,channel,5,(regAddr>>3)&0xe0); //write upper address bits
	
	WriteReg (portID,channel,6,(data<<3)&0xff);
	WriteReg (portID,channel,7,(data>>5)&0xff);
	WriteReg (portID,channel,8,(data>>13)&0xff);
	WriteReg (portID,channel,9,(data>>21)&0xff);
	
	WriteReg (portID,channel,10,regAddr&0xff); //write lower address bits  
}

unsigned int ReadRam (unsigned short portID, unsigned char channel, unsigned short regAddr)
{
	unsigned char reg; 
	unsigned int RegVal;

    RAMwait(portID,channel);
	WriteReg (portID,channel,5,(regAddr>>3)&0xe0); //write upper address bits
	
	WriteReg (portID,channel,10,regAddr&0xff); //write lower address bits
	
	RAMwait(portID,channel);
	
	reg = ReadReg (portID,channel,6);
	RegVal = reg>>3;
	reg = ReadReg(portID,channel,7);
	RegVal |= ((unsigned long)reg)<<5;
	reg = ReadReg(portID,channel,8);
	RegVal |= ((unsigned long)reg)<<13;
	reg = ReadReg(portID,channel,9);
	RegVal |= ((unsigned long)reg)<<21;
	
	return RegVal;
}

EXPORT_SYMBOL(ath_spi_si3217x_reset);
EXPORT_SYMBOL(spi_si3217x_read8);
EXPORT_SYMBOL(spi_si3217x_write8);

module_init(ath_spi_si3217x_init);
module_exit(ath_spi_si3217x_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Atheros SoC SPI Driver");

