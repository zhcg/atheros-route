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

#include <linux/ip.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv4.h>

#include <atheros.h>

extern int key_sw4_state;

static int num=3;
module_param(num,int,S_IRUGO);

#define B6_V3

#define ENABLE	1
#define DISABLE	0

#define DELAY_TIME 15
//SPI_CS1
#define ATH_SPI1_CE_LOW			0x50000 
#define ATH_SPI1_CE_HIGH		0x50100	

#define CPOL	0	
#define CPHA	1	

#define SM1_ENCRYPT	1
#define SM1_DECRYPT	2

#define SM1_CBC	1
#define SM1_ECB	2
#define SM1_CFB	3
#define SM1_OFB	4

enum{
	CMD_GET_RANDOM=1,
	CMD_RSA_INIT,
	CMD_GET_PUB_KEY,

	CMD_PUB_KEY,
	CMD_PRI_KEY,
	CMD_SM1,

	CMD_GET_VERSION,
}cmd;

/*
 * primitives
 */

#define ath_be_msb(_val, __i) (((_val) & (1 << (7 - __i))) >> (7 - __i))
#if (CPHA == 1)
/* CPHA = 1 */
#define ath_spi_bit_banger(_byte)	do {				\
	int _i;								\
	for(_i = 0; _i < 8; _i++) {					\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_HIGH | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_HIGH | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_LOW | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_LOW | ath_be_msb(_byte, _i));	\
	}								\
} while(0)

#else
/* CPHA = 0 */
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
#endif

#define ath_spi_delay_8()	ath_spi_bit_banger(0)


extern void ath_flash_spi_down(void);
extern void ath_flash_spi_up(void);

static void ath_spi_enable(void)
{
	ath_reg_wr_nf(ATH_SPI_FS, 1);
#if (CPOL == 0)
	/* CPOL = 0 */
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS );
#else
	/* CPOL = 1 */
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
#endif
}
static void ath_spi_done(void)
{
	ath_reg_wr_nf(ATH_SPI_FS, 0);
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
#if (CPOL == 0)
		/* CPOL = 0 */
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_ENABLE_1 );
#else
		/* CPOL = 1 */
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_ENABLE_1 | ATH_SPI_CLK_HIGH);
#endif
	} else  {
#if (CPOL == 0)
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
#else
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS | ATH_SPI_CLK_HIGH);
#endif
	}
}

int get_gpio_val(int gpio)
{
	return ((1 << gpio) & (ath_reg_rd(ATH_GPIO_IN)));
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

void csc03_read(uint8_t *prbuf, int len)
{
	int i;
	uint8_t ch;

	ath_flash_spi_down();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);

	ath_spi_enable();
	spi_chip_select(ENABLE);

	for (i = 0; i < len; i++) {
#if (CPHA == 1)
		ch = (((spi_read() << 1) & 0xFF) | (get_gpio_val(8) >> 8));  
#else
		ch = spi_read();
#endif
		*(prbuf + i) = ch;
	}
	
	spi_chip_select(DISABLE);	
	ath_spi_done();	

	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();
}

void csc03_write(uint8_t *pwbuf, int len)
{
	int i;
	uint8_t ch;

	ath_flash_spi_down();
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x2af);
	
	ath_spi_enable();
	spi_chip_select(ENABLE);

	for (i = 0; i < len; i++) {
		ch = *(pwbuf + i);
		spi_write(ch);
	}

	spi_chip_select(DISABLE);
	ath_spi_done();
	
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x243);
	ath_flash_spi_up();
}

int csc03_gpio_init(void)
{
	uint32_t rddata;
	uint32_t ret;
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
#endif
	return 0;
}

int csc03_reset(int status)
{
	uint32_t rddata;
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

uint8_t csc03_send_command_then_read(uint8_t *command, uint8_t cmd_len, uint8_t *data_in, uint8_t *data_out, uint16_t len)
{
	uint8_t pack_sum=0;
	uint8_t pack_index=0;
	uint16_t data_len=0;
	uint8_t respond[8]={0};
	int i,j;
	uint8_t *recv;

	/* delay for csc03 enc & dec, not apply to rsa & pubkey */
	mdelay(1);
	csc03_write(command, cmd_len);
	csc03_write(data_in, len);

	mdelay(1);
	csc03_read(respond, sizeof(respond));
/*
	for(i=0;i<cmd_len;i++)
		printk("%02x ", *(command+i));
	printk("\n");

	for(i=0;i<len;i++)
		printk("%02x ", *(data_in+i));
	printk("\n");

	for(i=0;i<8;i++)
		printk("%02x ", *(respond+i));
	printk("\n");
*/
	if (respond[0] == 0xaa && respond[1] == 0x55)
	{
		pack_sum = respond[2];
		pack_index = respond[3];
		cmd = respond[4] * 256 + respond[5];
	//	printk("csc03 cmd: %d\n", cmd);
		data_len = respond[6] * 256 + respond[7];

		recv = kmalloc(data_len*(pack_sum+1), GFP_KERNEL);
		if (pack_sum == 0)
		{
			csc03_read(recv, data_len);

			if (cmd == CMD_SM1)
				memcpy(data_out, recv+1, data_len-1);
			else
				memcpy(data_out, recv, data_len);
			kfree(recv);
			return 0;
		}
		else
		{
			csc03_read(recv, data_len);

			/* must add ? */
			mdelay(1);

			for (i=1; i<pack_sum; i++)
			{
			
				if (pack_index == i && pack_sum != pack_index)
				{
					csc03_read(respond, sizeof(respond));
				}

				if (respond[0] == 0xaa && respond[1] == 0x55)
				{
					pack_sum = respond[2];
					pack_index = respond[3];
					cmd = respond[4] * 256 + respond[5];
				//	printk("csc03 cmd: %d\n", cmd);
					data_len = respond[6] * 256 + respond[7];
					csc03_read(&recv[i*512], data_len);

					if (pack_sum == pack_index)
					{
						memcpy(data_out, &recv[1], len);
						kfree(recv);
						return 0;
					}

				}
				else
					return -1;
			}
		
		}
	}
	else
		return -1;
}

uint8_t csc03_get_version(uint8_t *ver, uint16_t len)
{
	uint8_t command[] = {0xAA,0x55,0x00,0x00,0x00,CMD_GET_VERSION,len/256,len%256};
	return csc03_send_command_then_read(command, sizeof(command), ver, ver, len);
}

uint8_t csc03_get_random(uint8_t *rand, uint16_t len)
{
	uint8_t command[] = {0xAA,0x55,0x00,0x00,0x00,CMD_GET_RANDOM,0x00,0x01,len};
	return csc03_send_command_then_read(command, sizeof(command), rand, rand, len);
}

uint8_t csc03_init_rsa(uint8_t *data, uint16_t len)
{
	uint8_t command[] = {0xAA,0x55,0x00,0x00,0x00,CMD_RSA_INIT,len/256,len%256};
	return csc03_send_command_then_read(command, sizeof(command), data, data, len);
}

uint8_t csc03_get_pubkey(uint8_t *data, uint16_t len)
{
	uint8_t command[] = {0xAA,0x55,0x00,0x00,0x00,CMD_GET_PUB_KEY,len/256,len%256};
	return csc03_send_command_then_read(command, sizeof(command), data, data, len);
}

uint8_t csc03_encrypt(uint8_t *data_in, uint8_t *data_out, uint16_t len)
{
	uint8_t command[42] = {0xAA,0x55,0x00,0x00,0x00,CMD_SM1,len/256,len%256+34,SM1_ENCRYPT,SM1_ECB};
	return csc03_send_command_then_read(command, sizeof(command), data_in, data_out, len);
}

uint8_t csc03_decrypt(uint8_t *data_in, uint8_t *data_out, uint16_t len)
{
	uint8_t command[42] = {0xAA,0x55,0x00,0x00,0x00,CMD_SM1,len/256,len%256+34,SM1_DECRYPT,SM1_ECB};
	return csc03_send_command_then_read(command, sizeof(command), data_in, data_out, len);
}


/* NF_INET_FORWARD */
unsigned int forward_hook(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int i,tcp_data_len,tcp_len;
	struct iphdr *iph;
	struct tcphdr *tcph;
	uint8_t *tcp_data;
	int ret;

	if (!skb)  
		return NF_ACCEPT;
	if(skb->protocol != htons(0x0800)) //查看是否是IP数据包（排除ARP干扰）
		return NF_ACCEPT; 

	iph = ip_hdr(skb);
	tcph = (void *)iph + iph->ihl*4;
	tcp_data_len = ntohs(iph->tot_len) - iph->ihl*4 - tcph->doff*4;
	tcp_data = (unsigned char *)tcph + tcph->doff*4;
	tcp_len = skb->len - iph->ihl*4;

	if(tcph->source == 90 || tcph->dest == 90)
	{
#if 1 
		if (tcp_data_len > 800 && key_sw4_state !=0)
		{
		//	printk("tcp ~ data len:%d\n",tcp_data_len);

		//	ret=csc03_encrypt(tcp_data, tcp_data, 32);
		//	if(ret)
		//	{
		//		printk("enc error\n");
		//		return NF_ACCEPT;
		//	}
		
	//		for(i=0; i<tcp_data_len; i++)
	//		{
	//			*(tcp_data + i) = ~*(tcp_data + i)&0xFF;
	////			printk("%02x ", *(tcp_data + i));
	//		}
	//		printk("\n");

			ret=csc03_decrypt(tcp_data, tcp_data, 32);
			if(ret)
			{
				printk("enc error\n");
				return NF_ACCEPT;
			}

			if (tcp_data_len !=0)
			{
				tcph->check = 0;
				tcph->check = tcp_v4_check(tcp_len,iph->saddr, iph->daddr,
						csum_partial(tcph,tcp_len, 0));
			}
		}
#endif
	}

	return NF_ACCEPT;
}

struct nf_hook_ops forward_ops =
{
	.hook = forward_hook,
	.pf = PF_INET,
	.hooknum = NF_INET_FORWARD,
	.priority = NF_IP_PRI_FIRST
};

static int __init ath_spi_csc03_init(void)
{
	int i,j,ret;
	uint8_t version[20];
	uint8_t random[16];
	uint8_t rsa[1];
	uint8_t pubkey[256+4+1];
	uint8_t data[16]={0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
	uint8_t enc_data[16];
	uint8_t dec_data[16];

	csc03_gpio_init();
//	csc03_get_version(version, 20);
//	printk("CSC03 version: %s\n", version);
//
//	csc03_get_random(random, 16);
//	printk("CSC03 random:\n");
//	for(i=0;i<sizeof(random);i++)
//		printk("0x%02x ", *(random+i));
//	printk("\n");


while(num--)
{
	printk("encrypt data:\n");
	ret=csc03_encrypt(data, data, sizeof(data));
	if(ret)
		printk("enc error\n");
	for(i=0;i<sizeof(enc_data);i++)
		printk("%02x ", *(data+i));
	printk("\n");

	printk("decrypt data:\n");
	ret=csc03_decrypt(data, data, sizeof(data));
	if(ret)
		printk("dec error\n");
//	for(i=0;i<sizeof(dec_data);i++)
//		printk("%02x ", *(data+i));
//	printk("\n");
}

	for(i=0;i<sizeof(data);i++)
		printk("%02x ", *(data+i));
	printk("\n");
/*
	csc03_init_rsa(rsa, sizeof(rsa));
	printk("csc03 init rsa: 0x%02x\n", *rsa);

	csc03_get_pubkey(pubkey, sizeof(pubkey));
	for(i=0;i<sizeof(pubkey);i++)
		printk("0x%02x ", *(pubkey+i));
	printk("\n");
*/
	printk("ath_spi_csc03_init %d\n", key_sw4_state);

	printk("hook_init()======================\n");
	nf_register_hook(&forward_ops);

	return 0;
}

static void __exit ath_spi_csc03_exit(void)
{
	printk("ath_spi_csc03_exit\n");

	printk("hook_exit()=====================\n");
	nf_unregister_hook(&forward_ops);
}

EXPORT_SYMBOL(csc03_encrypt);
EXPORT_SYMBOL(csc03_decrypt);

module_init(ath_spi_csc03_init);
module_exit(ath_spi_csc03_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Atheros SoC SPI Driver");

