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
#include <linux/interrupt.h>
#include <linux/time.h>


#include <atheros.h>
#ifdef  CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <linux/workqueue.h> 

struct workqueue_struct *doint_wq; 
struct work_struct doint_work;

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
static struct fasync_struct *button_async; 

static char dev_id[] = "csc03";

#define B6_V3
//#ifdef B6_V3
//#undef B6_V3
//#endif


#define SPI_DEV_NAME	"spiS2"
#define SPI_REG_READ	1
#define SPI_REG_WRITE	2
#define SPI_RAM_READ	3
#define SPI_RAM_WRITE	4
#define SPI_RESET		5
#define SPI_HOOK		6

#define ENABLE	1
#define DISABLE	0

#define DELAY_TIME 15
//SPI_CS1
#define ATH_SPI1_CE_LOW			0x50000 
#define ATH_SPI1_CE_HIGH		0x50100	

#define CSC03_INT_GPIO  14 

typedef struct si_ram_data {
	unsigned short sid;
	unsigned char channel;
	unsigned short ramAddr;
	unsigned int data;
} CSC03_RAM_DATA;

typedef struct si_reg_data {
	unsigned short sid;
	unsigned char channel;
	unsigned char regAddr;
	unsigned char data;
} CSC03_REG_DATA;

#define BUFLEN 4096 
typedef struct {
	unsigned char buf[BUFLEN];
	unsigned int write_point;
	unsigned int read_point;
} CSC03_DATA_BUFFER;

CSC03_DATA_BUFFER tx_data = {{0},0,0};
CSC03_DATA_BUFFER rx_data = {{0},0,0};

/*
 * primitives
 */

#define ath_be_msb(_val, __i) (((_val) & (1 << (7 - __i))) >> (7 - __i))

#define ath_spi_bit_banger(_byte)	do {				\
	int _i;								\
	for(_i = 0; _i < 8; _i++) {					\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_LOW | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_LOW | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_HIGH | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_HIGH | ath_be_msb(_byte, _i));	\
	}								\
} while(0)


#define ath_spi_delay_8()	ath_spi_bit_banger(0)



#define CNUM_TO_CID_QUAD(channelNumber)   (((channelNumber<<4)&0x10)|((channelNumber<<2)&0x8)|((channelNumber>>2)&0x2)|((channelNumber>>4)&0x1)|(channelNumber&0x4))

int spidrv_major =  217;
//int spich = 0;
void WriteReg (unsigned short portID, unsigned char channel, unsigned char regAddr, unsigned char data);
unsigned char ReadReg (unsigned short portID,  unsigned char channel,  unsigned char regAddr);
void WriteRam (unsigned short portID, unsigned char channel, unsigned short regAddr, unsigned int data);
unsigned int ReadRam (unsigned short portID, unsigned char channel, unsigned short regAddr);
int ath_spi_csc03_reset(int status);


extern void ath_flash_spi_down(void);
extern void ath_flash_spi_up(void);

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

int get_gpio_val(int gpio)
{
	return ((1 << gpio) & (ath_reg_rd(ATH_GPIO_IN)));
}

u8 spi_csc03_read8(unsigned short sid, unsigned char cid, unsigned char reg)
{
	unsigned char value;
	unsigned char regCtrl = CNUM_TO_CID_QUAD(cid)|0x60;
	
	ath_flash_spi_down();
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
//	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
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
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
//	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();	
	return value;
}

void spi_csc03_write8(unsigned short sid, unsigned char cid, unsigned char reg, unsigned char value)
{ 
	unsigned char regCtrl = CNUM_TO_CID_QUAD(cid)|0x20;
	ath_flash_spi_down();
	
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
//	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
	
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
	
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
//	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	
	ath_flash_spi_up();
}
 unsigned char ReadReg (unsigned short portID,  unsigned char channel,  unsigned char regAddr)
{
	
	return spi_csc03_read8(portID, channel, regAddr);
	
}

 void WriteReg (unsigned short portID, unsigned char channel, unsigned char regAddr, unsigned char data)
{	
	spi_csc03_write8(portID, channel, regAddr, data);
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

#if 1
void D01_WriteByte(unsigned char addr, unsigned char value)
{
	unsigned char regCtrl = CNUM_TO_CID_QUAD(0)|0x20;
	ath_flash_spi_down();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
	
//	ath_spi_enable();
//	spi_chip_select(ENABLE);
//	spi_write(regCtrl);
//	spi_chip_select(DISABLE);
	ath_spi_enable();
	spi_chip_select(ENABLE);
	spi_write(addr);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(value);
	spi_chip_select(DISABLE);
	ath_spi_done();

	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();	
}
void D01_WriteHalfWord(unsigned char addr, unsigned short value)
{
	unsigned char regCtrl = CNUM_TO_CID_QUAD(0)|0x20;
	ath_flash_spi_down();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);

//	ath_spi_enable();
//	spi_chip_select(ENABLE);
//	spi_write(regCtrl);
//	spi_chip_select(DISABLE);
	ath_spi_enable();
	spi_chip_select(ENABLE);
	spi_write(addr);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(((unsigned char *)&value)[0]);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(((unsigned char *)&value)[1]);
	spi_chip_select(DISABLE);
	ath_spi_done();

	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();	
	//mdelay(DELAY_TIME);
}
unsigned char D01_ReadByte(unsigned char addr)
{
	unsigned char ret = 0;
	unsigned char regCtrl = CNUM_TO_CID_QUAD(0)|0x60;
	
	ath_flash_spi_down();

	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
//	ath_spi_enable();
//	spi_chip_select(ENABLE);
//	spi_write(regCtrl);
//	spi_chip_select(DISABLE);
	ath_spi_enable();
	spi_chip_select(ENABLE);
	spi_write(addr);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	ret = spi_read();
	spi_chip_select(DISABLE);	
	ath_spi_done();	
	
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();	

	return ret;
}
unsigned short D01_ReadHalfWord(unsigned char addr)
{
	unsigned short ret = 0;
	unsigned char regCtrl = CNUM_TO_CID_QUAD(0)|0x60;
	
	ath_flash_spi_down();

	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
//	ath_spi_enable();
//	spi_chip_select(ENABLE);
//	spi_write(regCtrl);
//	spi_chip_select(DISABLE);
	ath_spi_enable();
	spi_chip_select(ENABLE);
	spi_write(addr);
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	(((unsigned char *)&ret)[0]) = spi_read();
	spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	(((unsigned char *)&ret)[1]) = spi_read();
	spi_chip_select(DISABLE);	
	ath_spi_done();	
	
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();	
	//mdelay(DELAY_TIME);

	return ret;
}
/*
**函数名称：D01_Init
**函数功能：给CSC03发送复位指令（0x01，no data）复位CSC03
*/
void D01_Reset(void)
{
	ath_flash_spi_down();

	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
	//D01_WriteByte(0x01, 0);
	ath_spi_enable();
	spi_chip_select(ENABLE);
	spi_write(0x01);
	spi_chip_select(DISABLE);
	ath_spi_done();

	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();	
	//mdelay(DELAY_TIME);
}
/*
**函数名称：D01_PowerUp
**函数功能：CSC03上电操作，首先对地址E0（通用控制寄存器）0x180（bit8&bit7置位），至少20ms后，bit7清除
*/
void D01_PowerUp(void)
{
	D01_WriteHalfWord(0xE0, 0x0180);
	//mdelay(DELAY_TIME);
	mdelay(50);
	D01_WriteHalfWord(0xE0, 0x0541);
}
/*
**函数名称：D01_SetTxMode
**函数功能：配置CSC03发送模式
*/
void D01_SetTxMode(void)
{
	D01_WriteHalfWord(0xE1, 0xA016);
}
/*
**函数名称：D01_SetRxMode
**函数功能：配置CSC03接收模式
*/
void D01_SetRxMode(void)
{
	D01_WriteHalfWord(0xE2, 0xA036);
}

/*
**函数名称：D01_SendData
**函数功能：CSC03发送数据
*/
void D01_SendData(unsigned char data)
{
	D01_WriteByte(0xE3, data);
}
/*
**函数名称：D01_RcvData
**函数功能：
*/
unsigned char D01_RcvData(void)
{
	return D01_ReadByte(0xE5);
}

/*
**函数名称：D01_SendData2
**函数功能：CSC03发送数据
*/
void D01_SendData2(unsigned short data)
{
	D01_WriteHalfWord(0xE3, data);
}
/*
**函数名称：D01_RcvData2
**函数功能：
*/
unsigned short D01_RcvData2(void)
{
	return D01_ReadHalfWord(0xE5);
}
/*
**函数名称：D01_GetStatusReg
**函数功能：获取CSC03状态寄存器的值，读取寄存器E6的值
*/
unsigned short D01_GetStatusReg(void)
{
	return D01_ReadHalfWord(0xE6);
}

/*
**函数名称：D01_SetSetQAM
**函数功能：配置CSC03传输质量寄存器
*/
void D01_SetSetQAM(void)
{
	D01_WriteHalfWord(0xEA, 0x0017);
}
/*
**函数名称：D01_GetQAM
**函数功能：获取自动模式下信号质量寄存器的值，读取寄存器EA的值
*/
unsigned short D01_GetQAM(void)
{
	return D01_ReadHalfWord(0xEB);
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
	CSC03_REG_DATA *reg_data;
	CSC03_RAM_DATA *ram_data;

	switch (cmd) {
	case SPI_REG_READ:
		reg_data = (CSC03_REG_DATA*)arg;
		sid = reg_data->sid;
		cid = reg_data->channel;
		regAddr = reg_data->regAddr;
		regData = D01_ReadByte(regAddr);
		reg_data->data = regData;
//		printk("SPI_SI_REG_READ :: 0x%02x : 0x%02x\n", regAddr, regData);
		break;
	case SPI_REG_WRITE:
		reg_data = (CSC03_REG_DATA*)arg;
		sid = reg_data->sid;
		cid = reg_data->channel;
		regAddr = reg_data->regAddr;
		regData = reg_data->data;
		D01_WriteByte(regAddr, regData);
//		printk("SPI_SI_REG_WRITE :: 0x%02x : 0x%02x\n", regAddr, regData);
		break;
	case SPI_RAM_READ:
		ram_data = (CSC03_RAM_DATA*)arg;
		sid = ram_data->sid;
		cid = ram_data->channel;
		ramAddr = ram_data->ramAddr;
		ramData = D01_ReadHalfWord(ramAddr);
		ram_data->data = ramData;
	//	arg = D01_GetStatusReg();
	//	printk("SPI_SI_RAM_READ :: 0x%08x\n", arg);
//		printk("SPI_SI_RAM_READ :: 0x%04x : 0x%08x\n", ramAddr, ramData);
		break;
	case SPI_RAM_WRITE:
		ram_data = (CSC03_RAM_DATA*)arg;
		sid = ram_data->sid;
		cid = ram_data->channel;
		ramAddr = ram_data->ramAddr;
		ramData = ram_data->data;
		D01_WriteHalfWord(ramAddr, ramData);
//		printk("SPI_SI_RAM_WRITE :: 0x%04x : 0x%08x\n", ramAddr, ramData);
		break;
	case SPI_RESET:
		D01_Reset();
//		printk("SPI_SI_RESET :: \n");
		break;
	case SPI_HOOK:
		if (arg)
			ath_spi_csc03_reset(1);
		else
			ath_spi_csc03_reset(0);
		rx_data.write_point=0;
		rx_data.read_point=0;
		tx_data.write_point=0;
		tx_data.read_point=0;
		break;
	default:
//		si_spi_test();
		printk("ath_spi_csc03: command format error\n");
	}

	return 0;
}

static int spidrv_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned int err;
	unsigned char *tmp;
	int size;
	int valid_len;
	int i;

	//printk("%s:\n", __func__);

	valid_len = BUFLEN - tx_data.write_point;
	//printk("%s: tx_data write_p=%d read_point=%d\n", __func__, tx_data.write_point, tx_data.read_point);
	if (valid_len >= count)
	{
		err = copy_from_user((char *)tx_data.buf + tx_data.write_point, buf, count);
		if (valid_len > count)
			tx_data.write_point += count;
		if (valid_len == count)
			tx_data.write_point = 0;
		size = count;
	}
	else
	{
		tmp = kmalloc(count, GFP_KERNEL);
		err = copy_from_user((char *)tmp, buf, count);
		memcpy(tx_data.buf + tx_data.write_point, tmp, valid_len);
		tx_data.write_point = 0;
		memcpy(tx_data.buf + tx_data.write_point, tmp+valid_len, count-valid_len);
		tx_data.write_point += count-valid_len;
		kfree(tmp);
		size = count;
	}

	return err ? -EINVAL : size;
}

static int spidrv_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	unsigned int err;
	int size;
	int valid_len;

	//printk("%s:\n", __func__);
	
	valid_len = rx_data.write_point - rx_data.read_point;
	//printk("%s: rx_data write_p=%d read_point=%d\n", __func__, rx_data.write_point, rx_data.read_point);
	if (valid_len > 0)
	{
		if(valid_len >= count)
		{
			err = copy_to_user(buff, (char *)(rx_data.buf + rx_data.read_point), count);
			rx_data.read_point += count;
			size = count;
		}
		else
		{
			err = copy_to_user(buff, (char *)(rx_data.buf + rx_data.read_point), valid_len);
			rx_data.read_point += valid_len;
			size = valid_len;
		}
	}
	else if (valid_len < 0)
	{
		if(BUFLEN-rx_data.read_point >= count)
		{
			err = copy_to_user(buff, (char *)(rx_data.buf + rx_data.read_point), count);
			if (BUFLEN-rx_data.read_point > count)
				rx_data.read_point += count;
			if (BUFLEN-rx_data.read_point == count)
				rx_data.read_point = 0;
			size = count;
		}
		else
		{
			err = copy_to_user(buff, (char *)(rx_data.buf + rx_data.read_point), BUFLEN-rx_data.read_point);
			size = BUFLEN-rx_data.read_point;
			rx_data.read_point = 0;
		}
	}
	else
	{
		err = copy_to_user(buff, (char *)(rx_data.buf + rx_data.read_point), 0);
		//D01_SendData2(0x0A0A);
		//disable_irq(ATH_GPIO_IRQn(CSC03_INT_GPIO));		
		//queue_work(doint_wq ,&doint_work);
		size = 0;
	}

	return err ? -EFAULT : size;
}

static int spidrv_fasync (int fd, struct file *filp, int on)  
{  
        printk("driver: fifth_drv_fasync\n");  
          
        return fasync_helper(fd, filp, on, &button_async);  
}

struct file_operations spidrv_fops = {
    ioctl:      spidrv_ioctl,
    write:      spidrv_write,
    read:		spidrv_read,
    open:       spidrv_open,
    release:    spidrv_release,
    fasync:     spidrv_fasync, 
};

int ath_spi_csc03_reset(int status)
{
	unsigned int rddata;
#ifdef B6_V3
	ath_reg_rmw_clear(ATH_GPIO_OE, 1<<12);

	rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION4);
	rddata = rddata & 0xffffff;
//	rddata = rddata | ATH_GPIO_OUT_FUNCTION4_ENABLE_GPIO_19(0x00);
	ath_reg_wr(ATH_GPIO_OUT_FUNCTION4, rddata);

	if (status) {
		ath_reg_rmw_clear(ATH_GPIO_OUT, 1<<12);
	} else  {
		ath_reg_rmw_set(ATH_GPIO_OUT, 1<<12);
	}
#endif
	return 0;
}


int ath_spi_csc03_gpio_init(void)
{
	unsigned int rddata;
#ifdef B6_V3
	//set gpio17 used as spi_cs for csc03 on B6_V3 board
	ath_reg_rmw_clear(ATH_GPIO_OE, 1<<17);
	
    rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION4);
    rddata = rddata & 0xffff00ff;
    rddata = rddata | ((0x07) << 8);
    ath_reg_wr(ATH_GPIO_OUT_FUNCTION4, rddata);
	
	/* set gpio11 used as csc03 power */
	ath_reg_rmw_clear(ATH_GPIO_OE, 1<<11);
	
    rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION2);
    rddata = rddata & 0x00ffffff;
    ath_reg_wr(ATH_GPIO_OUT_FUNCTION2, rddata);

	ath_reg_rmw_clear(ATH_GPIO_OUT, 1<<11);

	//set GPIO2 for input and interrupt 
	rddata = ath_reg_rd(ATH_GPIO_OE);
	rddata |= (1 << CSC03_INT_GPIO);
	ath_reg_wr(ATH_GPIO_OE, rddata);
	
	//set GPIO as interrupt
	//edge triggered
	rddata = ath_reg_rd(ATH_GPIO_INT_TYPE);
	rddata |= (1 << CSC03_INT_GPIO);
	//rddata &= ~(1 << CSC03_INT_GPIO);
	ath_reg_wr(ATH_GPIO_INT_TYPE, rddata);
	
	//failing edge
	//ath_reg_rmw_set(ATH_GPIO_INT_POLARITY, (1 << STM32_INT_GPIO));

	rddata = ath_reg_rd(ATH_GPIO_INT_POLARITY);
	rddata &= ~(1 << CSC03_INT_GPIO);
	ath_reg_wr(ATH_GPIO_INT_POLARITY, rddata);
	
	ath_reg_rmw_set(ATH_GPIO_INT_ENABLE, (1 << CSC03_INT_GPIO));
	

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

#if 1
void si_spi_test(void)
{
	int i;
	unsigned char regData;
	printk("SI :: Reset ......\n");
	ath_spi_csc03_reset(1);
	mdelay(1000);
	ath_spi_csc03_reset(0);
//	for(i = 0; i < 10; i++){
		//regData = ReadReg(0, 0x0, 0);
		regData = D01_ReadByte(0);
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

void doint_func(struct work_struct *work)
{ 
	unsigned short status = 0;
	unsigned short data = 0;
	int valid_len = 0;
	struct timeval time1;
	struct timeval time2;

ModemInterruptRepeat:
	//do_gettimeofday(&time1);
	//printk(KERN_INFO "%s start:: ~~~%lld.%lld~~~ \n",__func__,time1.tv_sec, time1.tv_usec/1000);
	status = D01_GetStatusReg();
	printk("DRIVER status=0x%04x\n",status);

	if ((status & 0x40) || (status & 0x20)!=0) //bit6 Rx character ready
	{
		data = D01_RcvData2();
		if (status & 0x02) //bit1 // Two Rx characters ready
		{
			memcpy(rx_data.buf + rx_data.write_point, &data, sizeof(data));
			rx_data.write_point += sizeof(data);
			printk("%s:DRIVER rrrrrrrrrrrrrrrrrrrecv data=0x%04x, rx_wp=%d, rx_rp=%d\n", __func__, data, rx_data.write_point, rx_data.read_point);
			if (rx_data.write_point > BUFLEN-1)
				rx_data.write_point = 0;
		}
	}

	if ((status & 0x1000)!=0 && (status & 0x0800)!=0) //bit12 Tx character ready && bit11 Tx underflow 1
	{
		valid_len = tx_data.write_point - tx_data.read_point;
		if (valid_len > 0)
		{
			if (valid_len >= sizeof(data))
			{
				memcpy(&data, tx_data.buf + tx_data.read_point, sizeof(data));
				tx_data.read_point += sizeof(data);
				D01_SendData2(data);
				printk("%s tttttttttttttx 1 data=0x%04x, tx_wp=%d, tx_rp=%d\n", __func__, data, tx_data.write_point, tx_data.read_point);
			}
		}
		if (valid_len < 0)
		{
			if (BUFLEN-tx_data.read_point > sizeof(data))
			{
				memcpy(&data, tx_data.buf + tx_data.read_point, sizeof(data));
				tx_data.read_point += sizeof(data);
				D01_SendData2(data);
				printk("%s tx 2 data=0x%04x, tx_wp=%d, tx_rp=%d\n", __func__, data, tx_data.write_point, tx_data.read_point);
			}
			else
			{
				memcpy(&data, tx_data.buf + tx_data.read_point, sizeof(data));
				tx_data.read_point = 0;
				D01_SendData2(data);
				printk("%s tx 3 data=0x%04x, tx_wp=%d, tx_rp=%d\n", __func__, data, tx_data.write_point, tx_data.read_point);
			}
		}
	}

	if (!get_gpio_val(CSC03_INT_GPIO))
		goto ModemInterruptRepeat;

	kill_fasync(&button_async, SIGIO, POLL_IN);

	//do_gettimeofday(&time2);
	//printk(KERN_INFO "%s end:: ~~~%lld.%lld~~~ \n",__func__,time2.tv_sec, time2.tv_usec/1000);
	enable_irq(ATH_GPIO_IRQn(CSC03_INT_GPIO));
} 


static irqreturn_t rxtx_irq_handler(int irq, void *irqaction)
{
	disable_irq_nosync(ATH_GPIO_IRQn(CSC03_INT_GPIO));		
	queue_work(doint_wq ,&doint_work);
	return IRQ_HANDLED;
}


void rxtx_request_irq(void)
{
	int ret;
	
	ret = request_irq(ATH_GPIO_IRQn(CSC03_INT_GPIO), rxtx_irq_handler, IRQF_TRIGGER_FALLING , "csc03", dev_id); //IRQF_SHARED IRQF_TRIGGER_FALLING
	if(ret)
	{
		printk("SPI: GPIO_IRQ %d is not free.\n", CSC03_INT_GPIO);
		return ;
	}
}

static int __init ath_spi_csc03_init(void)
{

	unsigned short status1=0;

	printk("ath_spi_csc03_init\n");
	ath_spi_csc03_gpio_init();

	D01_WriteHalfWord(0xAF, 0x0F0F);
	D01_WriteHalfWord(0xAF, 0xAAAA);
	D01_WriteHalfWord(0xAF, 0x1111);

//	rxtx_request_irq();
//	INIT_WORK(&doint_work, doint_func);
//	doint_wq = create_workqueue("doint");

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

static void __exit ath_spi_csc03_exit(void)
{
	printk("ath_spi_csc03_exit\n");
	
//    flush_workqueue(doint_wq);
//    destroy_workqueue(doint_wq);
//	free_irq(ATH_GPIO_IRQn(CSC03_INT_GPIO), dev_id);
#ifdef  CONFIG_DEVFS_FS
	devfs_unregister_chrdev(spidrv_major, SPI_DEV_NAME);
	devfs_unregister(devfs_handle);
#else
	unregister_chrdev(spidrv_major, SPI_DEV_NAME);
#endif

}

EXPORT_SYMBOL(ath_spi_csc03_reset);
EXPORT_SYMBOL(spi_csc03_read8);
EXPORT_SYMBOL(spi_csc03_write8);

module_init(ath_spi_csc03_init);
module_exit(ath_spi_csc03_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Atheros SoC SPI Driver");

