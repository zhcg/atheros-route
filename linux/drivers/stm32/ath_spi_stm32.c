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

#include <atheros.h>
#include "ath_spi_stm32.h"
#ifdef  CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif


#define CNUM_TO_CID_QUAD(channelNumber)   (((channelNumber<<4)&0x10)|((channelNumber<<2)&0x8)|((channelNumber>>2)&0x2)|((channelNumber>>4)&0x1)|(channelNumber&0x4))

int spidrv_major =  217;
//int spich = 0;
static char dev_id[] = "stm32";

static atomic_t ath_fr_status = ATOMIC_INIT(0);
static wait_queue_head_t ath_fr_wq;

#define NUMBER 2054
//GPIO0 for cs1,GPIO1 for spi irq
#define STM32_CS_GPIO   0
#define STM32_INT_GPIO  1 

u8 spi_stm32_read8(unsigned short sid, unsigned char cid, unsigned char reg);
void spi_stm32_write8(unsigned short sid, unsigned char cid, unsigned char reg, unsigned char value);
u8 spi_stm32_read16(int sid, unsigned char cid, unsigned char reg);
void spi_stm32_write16(int sid, unsigned char cid, unsigned char reg, unsigned short value);
int ath_spi_stm32_reset(int status);


extern void ath_flash_spi_down(void);
extern void ath_flash_spi_up(void);


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

void si_spi_test(void)
{
	int i;
	unsigned char regData;
	printk("SI :: Reset ......\n");
	ath_spi_si3217x_reset(1);
	mdelay(1000);
	ath_spi_si3217x_reset(0);
	for(i = 0; i < 10; i++){
		regData = spi_stm32_read8(0, 0x0, i);
		printk("SI :: ID Register: 0x%x \n", regData);
		printk("SI :: Part Number: 0x%x \n", (regData & 0x38) >> 3);
		printk("SI :: Revision Identification: 0x%x \n", (regData & 0x07));
	}

	spi_stm32_write8(0, 0, 66, 0x55);
	regData = 0;
	regData = spi_stm32_read8(0, 0, 66);
	printk("SI :: USER_STAT Register: 0x%x \n", regData);
}

//接收数据环形缓冲区
struct recv_buf
{
	u8 buffer[NUMBER];	// 接收数据buffer
	u16  w_pos;			//写位置
	u16  r_pos;			//读位置
}circle_buffer;

/************************************************************/
/*	FUNCTION:	write_buf									*/
/*		write_data: 写入环形缓冲区的数据					*/	
/*	RETURN													*/
/*		0: write buffer failure								*/
/*		1: write buffer success                             */
/************************************************************/
int write_buf(u8 write_data)
{
	// 环形缓冲区满，不能写入
	if(circle_buffer.r_pos == (circle_buffer.w_pos + 1) % NUMBER)
	{
		//超时未读取缓冲区中的数据，导致缓冲区满，清除数据
		printk("buffer full!\n");
		memset(circle_buffer.buffer, 0, sizeof(circle_buffer.buffer));
		circle_buffer.w_pos = circle_buffer.r_pos = 0;
		return -1;
	}
	else	//环形缓冲区中空，插入数据
	{
		circle_buffer.buffer[circle_buffer.w_pos] = write_data;
		circle_buffer.w_pos++;
		circle_buffer.w_pos = circle_buffer.w_pos % NUMBER;

		return 1;
	}
}

/****************************************************************/
/*	FUNCTION													*/
/*		read_buf												*/
/*	return														*/
/*		0: read data failure									*/
/*		u8 data: read data success								*/
/****************************************************************/
u8 read_buf(void)
{
	u8 tmp;

	//环形缓冲区为空
	if(circle_buffer.w_pos == circle_buffer.r_pos)
	{
		return 0;
	}
	else	//环形缓冲区中有数据，读出来
	{
		tmp = circle_buffer.buffer[circle_buffer.r_pos];
		circle_buffer.r_pos++;
		//防止越界
		circle_buffer.r_pos = circle_buffer.r_pos % NUMBER;
		return tmp;
	}
}

/*****************************************************************/
/* FUNCTION:													 */
/*			count_buffer_datalen							     */
/* RETURN:														 */
/*			统计环形缓冲区中可读的数据个数						 */
/*****************************************************************/
u16 count_buffer_datalen(void)
{
	if(circle_buffer.w_pos > circle_buffer.r_pos)
	{
		return (circle_buffer.w_pos - circle_buffer.r_pos);
	}
	else if(circle_buffer.w_pos == circle_buffer.r_pos)
	{
		return 0;
	}
	else
	{
		return (NUMBER- (circle_buffer.r_pos - circle_buffer.w_pos));
	}
}

int spidrv_open(struct inode *inode, struct file *filp)
{
	//printk("open spidrv!\n");
	memset(circle_buffer.buffer, 0, sizeof(circle_buffer.buffer));
	circle_buffer.w_pos = circle_buffer.r_pos = 0;

	return 0;
}

int spidrv_release(struct inode *inode, struct file *filp)
{
	return 0;
}
/*
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
		regData = spi_si3217x_read8(sid, cid, regAddr);
		reg_data->data = regData;
		printk("SPI_SI_REG_READ :: 0x%08x : 0x%08x\n", regAddr, regData);
		break;
	case SPI_SI_REG_WRITE:
		reg_data = (SI3217x_REG_DATA*)arg;
		sid = reg_data->sid;
		cid = reg_data->channel;
		regAddr = reg_data->regAddr;
		regData = reg_data->data;
		spi_si3217x_write8(sid, cid, regAddr, regData);
		printk("SPI_SI_REG_WRITE :: 0x%08x : 0x%08x\n", regAddr, regData);
		break;
	case SPI_SI_RAM_READ:
		ram_data = (SI3217x_RAM_DATA*)arg;
		sid = ram_data->sid;
		cid = ram_data->channel;
		ramAddr = ram_data->ramAddr;
		ramData = spi_si3217x_read16(sid, cid, ramAddr);
		ram_data->data = ramData;
		printk("SPI_SI_RAM_READ :: 0x%08x : 0x%016x\n", ramAddr, ramData);
		break;
	case SPI_SI_RAM_WRITE:
		ram_data = (SI3217x_RAM_DATA*)arg;
		sid = ram_data->sid;
		cid = ram_data->channel;
		ramAddr = ram_data->ramAddr;
		ramData = ram_data->data;
		spi_si3217x_write16(sid, cid, ramAddr, ramData);
		printk("SPI_SI_RAM_WRITE :: 0x%08x : 0x%016x\n", ramAddr, ramData);
		break;
	default:
		si_spi_test();
		printk("ath_spi_si3217x: command format error\n");
	}

	return 0;
}
*/
static ssize_t spidrv_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
	int i = 0;
	int read_count;
	unsigned char read_data;
	unsigned char *kbuf;

	read_count = count_buffer_datalen();
	if(read_count == 0)
		return 0;
	
	kbuf = (unsigned char *)kmalloc((read_count), GFP_KERNEL);
	if(NULL== kbuf){
		printk("alloc error!\n");
		return -ENOMEM;
	}
	for(i = 0; i < read_count; i++)
	{
		read_data = read_buf();
		kbuf[i]	= read_data;
	}

	if( copy_to_user(buf, kbuf, read_count)){
		printk("copy_to_user error, no enough memory!\n");
		return -1;
	}
	kfree(kbuf);

	return (read_count);
}

static ssize_t spidrv_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{
	int i = 0;
	unsigned char *kbuf;

	if(!count)
	{
		return 0;
	}
	
	kbuf = (unsigned char *)kmalloc(count, GFP_KERNEL);
	if(NULL == kbuf){
		printk("alloc error!\n");
		return -ENOMEM;
	}
    if ( copy_from_user ( kbuf, buf, count )){
        printk ( "copy_from_user error, no enough memory!\n" );
        return -1;
    }
	for(i = 0; i < count; i++)
	{
		spi_stm32_write8(0,0,0,kbuf[i]);
	}
	kfree(kbuf);
	
    return count;
}

struct file_operations spidrv_fops = {
//   ioctl:      spidrv_ioctl,
    open:       spidrv_open,
    release:    spidrv_release,
	read:       spidrv_read,
    write:      spidrv_write,
};


//set gpio for int and cs 
int ath_spi_stm32_gpio_init(void)
{
	unsigned int rddata;
	
	//1.Disable JTAG
	
	ath_reg_rmw_set(ATH_GPIO_FUNCTIONS,
			   ATH_GPIO_FUNCTION_JTAG_DISABLE);
		
	//2.TODO set GPIO0 output and cs1	
			   
	ath_reg_rmw_clear(ATH_GPIO_OE, 1 << STM32_CS_GPIO);
	rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION0);
	rddata &= 0xffffff00;
	rddata = rddata | ATH_GPIO_OUT_FUNCTION0_ENABLE_GPIO_0(0x07);
	ath_reg_wr(ATH_GPIO_OUT_FUNCTION0, rddata);

	//3.set GPIO1 for input and interrupt 
	
	//set GPIO as input
	
	rddata = ath_reg_rd(ATH_GPIO_OE);
	rddata |= (1 << STM32_INT_GPIO);
	ath_reg_wr(ATH_GPIO_OE, rddata);
	
	//set GPIO as interrupt
	//edge triggered
	rddata = ath_reg_rd(ATH_GPIO_INT_TYPE);
	rddata |= (1 << STM32_INT_GPIO);
	ath_reg_wr(ATH_GPIO_INT_TYPE, rddata);
	
	//failing edge
	//ath_reg_rmw_set(ATH_GPIO_INT_POLARITY, (1 << STM32_INT_GPIO));

	rddata = ath_reg_rd(ATH_GPIO_INT_POLARITY);
	rddata &= ~(1 << STM32_INT_GPIO);
	ath_reg_wr(ATH_GPIO_INT_POLARITY, rddata);
	
	ath_reg_rmw_set(ATH_GPIO_INT_ENABLE, (1 << STM32_INT_GPIO));
	
	
	//init signal
/* 	rddata = ath_reg_rd(ATH_SPI_SHIFT_CNT);
	if( rddata & (1<<27) )
	{
		printk("1111111");
		rddata &= ~(1 << 27);
		ath_reg_wr(ATH_SPI_SHIFT_CNT, rddata);
	}else
	{
		printk("0000000");
		rddata |= (1 << 27);
		ath_reg_wr(ATH_SPI_SHIFT_CNT, rddata);
	} */
	
	
	return 0;
}

static unsigned char high8bit (unsigned short data)
{
	return ((data>>8)&(0x0FF));
}

static unsigned char low8bit (unsigned short data)
{
	return ((unsigned char)(data & 0x00FF));
}

static unsigned short SixteenBit (unsigned char hi, unsigned char lo)
{
	unsigned short data = hi;
	data = (data<<8)|lo;
	return data;
}


static void ath_spi_enable(void)
{
	ath_reg_wr_nf(ATH_SPI_FS, 1);
	//[TODO2]ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
	// [TODO] ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
	//ath_reg_rmw_set(ATH_GPIO_OUT, 1<<0);
	//printk("SI :: ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH) \n");
	//ndelay(200);
}

static void ath_spi_done(void)
{
	
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
//	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
	ath_reg_wr(ATH_SPI_FS, 0);
	//ath_reg_rmw_set(ATH_GPIO_OUT, 1<<0);
	//printk("SI :: ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH) \n");
	
}


static void spi_chip_select(u8 enable)
{
	if (enable) {
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_ENABLE_1 | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_ENABLE_1 | ATH_SPI_CLK_HIGH);
		//ath_reg_rmw_clear(ATH_GPIO_OUT, 1<<0);
		

		
	} else  {
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		//ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		// [TODO] ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		// [TODO] ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		// [TODO] ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		// [TODO] ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		// [TODO] ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
		//ath_reg_rmw_set(ATH_GPIO_OUT, 1<<0);
	}		
}


static void spi_write(u8 data)
{
	ath_spi_bit_banger(data);
}


static u8 spi_read(void) 
{
	ath_spi_delay_8();
	return (ath_reg_rd(ATH_SPI_RD_STATUS) & 0xff);
}

int get_gpio_val(int gpio)
{
	return ((1 << gpio) & (ath_reg_rd(ATH_GPIO_IN)));
}

u8 spi_stm32_read8(unsigned short sid, unsigned char cid, unsigned char reg)
{
	unsigned char value;
	
	//wait_event_interruptible(ath_fr_wq, !(get_gpio_val(STM32_INT_GPIO)));
	//unsigned char regCtrl = CNUM_TO_CID_QUAD(cid)|0x60;
	ath_flash_spi_down();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
	//udelay(5);
	ath_spi_enable();
	//spi_chip_select(ENABLE);
	//spi_write(regCtrl);
	//spi_chip_select(DISABLE);
	//spi_chip_select(ENABLE);
	//spi_write(reg);
	//spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	value = spi_read();
	//udelay(5);
	printk("%x ",value);
	spi_chip_select(DISABLE);	
	ath_spi_done();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	//udelay(5);
	ath_flash_spi_up();	
	return value;
}

void spi_stm32_write8(unsigned short sid, unsigned char cid, unsigned char reg, unsigned char value)
{ 
	//unsigned char regCtrl = CNUM_TO_CID_QUAD(cid)|0x20;
	
	ath_flash_spi_down();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
	//udelay(5);
	ath_spi_enable();
	//spi_chip_select(ENABLE);
	//spi_write(regCtrl);
	//spi_chip_select(DISABLE);
	//spi_chip_select(ENABLE);
	//spi_write(reg);
	//spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(value);
	//udelay(5);
	//printk("2014-");
	spi_chip_select(DISABLE);
	ath_spi_done();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	//udelay(5);
	ath_flash_spi_up();
}

u8 spi_stm32_read16(int sid, unsigned char cid, unsigned char reg)
{
	unsigned char hi, lo;
	//spi_chip_select(ENABLE);
	//spi_write(cid);
	//spi_write(reg);
	//spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	hi = spi_read();
	lo = spi_read();
	spi_chip_select(DISABLE);
	return SixteenBit(hi,lo);
}

void spi_stm32_write16(int sid, unsigned char cid, unsigned char reg, unsigned short value)
{
	unsigned char hi, lo;
	
	hi = high8bit(value);
	lo = low8bit(value);
	//spi_chip_select(ENABLE);
	//spi_write(cid);
	//spi_write(reg);
	//spi_chip_select(DISABLE);
	spi_chip_select(ENABLE);
	spi_write(hi);
	spi_write(lo);
	spi_chip_select(DISABLE);
}



static irqreturn_t rxtx_irq_handler(int irq, void *irqaction)
{
	int i = 0;
	u8	msb_head = 0;
	u8	lsb_head = 0;
	u8  msb_length = 0;
	u8  lsb_length = 0;
	u8  data = 0;
	u16 length = 0;

	//TODO LED

	//if (atomic_read(&ath_fr_status)) {
		
			printk("do int!\n");
			
			if (get_gpio_val(STM32_INT_GPIO)) {
				printk("INT Hight!\n");
				return IRQ_HANDLED;
			}
			printk("INT FALLING!\n");
			atomic_dec(&ath_fr_status);
			disable_irq(ATH_GPIO_IRQn(STM32_INT_GPIO));			
		    //wake_up(&ath_fr_wq);
			
			msb_head = spi_stm32_read8(0,0,0);
			lsb_head = spi_stm32_read8(0,0,0);
			write_buf(msb_head);			//write the  MSB effective data to circle buffer
			write_buf(lsb_head);			//write the  LSB effective data to circle buffer
			msb_length = spi_stm32_read8(0,0,0);
			lsb_length = spi_stm32_read8(0,0,0);
			write_buf(msb_length);			//write the  MSB effective data to circle buffer
			write_buf(lsb_length);			//write the  LSB effective data to circle buffer

			length = (msb_length << 8) | lsb_length;
			for(i = 0; i < length; i++ )
			{
				data = spi_stm32_read8(0,0,0);
				write_buf(data);			//write the effective data to circle buffer
			}
			
			//printk("[luodp] INT_MASK %x \n",ath_reg_rd(ATH_GPIO_INT_MASK));
			//printk("[luodp] INT_Pending %x \n",ath_reg_rd(ATH_GPIO_INT_PENDING));
			enable_irq(ATH_GPIO_IRQn(STM32_INT_GPIO));

			/* clear int */
	//}
	return IRQ_HANDLED;
}



void rxtx_request_irq(void)
{
	int ret;
	
	//TODO LED int

	//MSG("start request irq!\n");
	ret = request_irq(ATH_GPIO_IRQn(STM32_INT_GPIO), rxtx_irq_handler, IRQF_TRIGGER_FALLING | IRQF_SHARED , "stm32", dev_id); //IRQF_SHARED IRQF_TRIGGER_FALLING
	if(ret)
	{
		printk("SPI: GPIO_IRQ %d is not free.\n", STM32_INT_GPIO);
		return ;
	}
	//init_waitqueue_head(&ath_fr_wq);
}


static int __init ath_spi_stm32_init(void)
{

	printk("ath_spi_stm32_init\n");
	ath_spi_stm32_gpio_init();

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
	//TODO add LED ?
	rxtx_request_irq();
	
	//ath_reg_wr(ATH_SPI_FS, 0);
	return 0;
}

static void __exit ath_spi_stm32_exit(void)
{
	printk("ath_spi_stm32_exit\n");
	free_irq(ATH_GPIO_IRQn(STM32_INT_GPIO), dev_id);
#ifdef  CONFIG_DEVFS_FS
	devfs_unregister_chrdev(spidrv_major, SPI_DEV_NAME);
	devfs_unregister(devfs_handle);
#else
	unregister_chrdev(spidrv_major, SPI_DEV_NAME);
#endif

}

module_init(ath_spi_stm32_init);
module_exit(ath_spi_stm32_exit);

module_param (spidrv_major, int, 0);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Atheros SoC SPI Driver");

