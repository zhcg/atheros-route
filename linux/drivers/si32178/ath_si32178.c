/* 
Copyright © 2012 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>

#include "atheros.h"
#include "ath_si32178.h"		/* local definitions */

void ath_slic_enable(void);

#define B6_V3 		//平台V3
//#ifdef B6_V3
//#undef B6_V3
//#endif

int ath_slic_major = 252;
int ath_slic_minor = 0;
int ath_slic_wide  = 0;
int ath_slic_slave = 0;

/* Default values of module params */
int ath_slic_max_slots = DEF_MAX_SLOTS_8;
int ath_slic_slots_en = DEF_SLOTS_EN_8;
int ath_slic_long_fs = DEF_FS_LONG_1;
int ath_slic_tx_sweep_pos = DEF_TX_SWEEP_POS_1;
int ath_slic_rx_sweep_pos = DEF_RX_SWEEP_POS_0;
int ath_slic_mclk_sel = DEF_MCLK_SEL_1; /* Default ref clk */ 

static DECLARE_WAIT_QUEUE_HEAD(wq_tx);
static DECLARE_WAIT_QUEUE_HEAD(wq_rx);

EXPORT_SYMBOL(wq_rx);
EXPORT_SYMBOL(wq_tx);

module_param(ath_slic_major, int, S_IRUGO);
module_param(ath_slic_minor, int, S_IRUGO);
module_param(ath_slic_wide , int, S_IRUGO);
module_param(ath_slic_slave, int, S_IRUGO);
module_param(ath_slic_max_slots, int, S_IRUGO);
module_param(ath_slic_slots_en, int, S_IRUGO);
module_param(ath_slic_long_fs , int, S_IRUGO);
module_param(ath_slic_tx_sweep_pos, int, S_IRUGO);
module_param(ath_slic_rx_sweep_pos, int, S_IRUGO);
module_param(ath_slic_mclk_sel, int, S_IRUGO);

MODULE_AUTHOR("Mughilan@QCA");
MODULE_LICENSE("Dual BSD/GPL");

int ath_slic_buf_size;

#define prev_tail(t) ({ (t == 0) ? (NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (NUM_DESC - 1)) ? 0 : (t + 1); })
#define space_valid(wt, rd) ({(wt >= rd) ? (wt-rd) : (NUM_DESC-rd+wt);})

//用户存放每一块数据来源标志 1 用户写 0 中断添加
typedef struct _ath_mbox_buf_flag{
	char user_add[NUM_DESC];
}ath_mbox_buf_flag;

//ath_mbox_buf_flag sc_rbuf_flag;
ath_mbox_buf_flag sc_pbuf_flag;

int slic_dma_open=0;

#define DO_AUDIO_FILTER		//侧音处理

#ifdef DO_AUDIO_FILTER
static int compare_times = 15;//默认比较数字半双工倍乘值
static int last_do_flag = 0;
static int extra_do_filter_count=2;

static int do_echo_filter=0;

//侧音处理

static int AudioDriverReadFilterDo(char *pplay_pos, char *precord_pos)
{
	int i;
	signed short *psh_start;
	unsigned int play_energy = 0;
	unsigned int record_energy = 0;
	int short_count;
	
	short_count = SLIC_BUF_SIZE/2;
	psh_start = (signed short *)pplay_pos;
	for(i = 0;i < short_count;i++)
	{
		play_energy += abs(psh_start[i]);			
	}
	
	play_energy /= short_count;
	play_energy *= compare_times;
	play_energy /= 100;
	
	psh_start = (signed short *)precord_pos;
	for(i = 0;i < short_count;i++)
	{
		record_energy += abs(psh_start[i]);			
	}

	record_energy /= short_count;

	if(play_energy >= record_energy)
	{
		for(i = 0;i < short_count;i++)
		{
			psh_start[i] = 0x00;
		}
		last_do_flag = extra_do_filter_count;
		return 0;
	}
	if(last_do_flag > 0)
	{
		for(i = 0;i < short_count;i++)
		{
			psh_start[i] = 0x00;
		}
		last_do_flag--;
	}
	return 0;
}
#endif

void ath_slic_clk(unsigned long frac, unsigned long pll)
{
    /*
     * Tick...Tick...Tick
     */
    ath_reg_wr(ATH_FRAC_FREQ_CONFIG, frac);
    ath_reg_wr(ATH_AUDIO_PLL_CONFIG, pll);
    ath_reg_wr(ATH_AUDIO_PLL_CONFIG, (pll & ~ATH_AUDIO_PLL_CFG_PWR_DWN));
}

void ath_slic_dpll(uint32_t kd, uint32_t ki)
{
	uint32_t	i = 0;

	do {
		ath_reg_rmw_clear(ATH_AUD_DPLL3_ADDRESS, ATH_AUD_DPLL3_DO_MEAS_SET(1));
		ath_reg_rmw_set(ATH_AUDIO_PLL_CONFIG, ATH_AUDIO_PLL_CFG_PWR_DWN);
		udelay(100);
		// Configure AUDIO DPLL
		ath_reg_rmw_clear(ATH_AUD_DPLL2_ADDRESS, ATH_AUD_DPLL2_KI_MASK | ATH_AUD_DPLL2_KD_MASK);
		ath_reg_rmw_set(ATH_AUDIO_DPLL2, ATH_AUD_DPLL2_KI_SET(ki) | ATH_AUD_DPLL2_KD_SET(kd));
		ath_reg_rmw_clear(ATH_AUD_DPLL3_ADDRESS, ATH_AUD_DPLL3_PHASE_SHIFT_MASK);
		ath_reg_rmw_set(ATH_AUD_DPLL3_ADDRESS, ATH_AUD_DPLL3_PHASE_SHIFT_SET(0x6));
		if (!is_ar934x_10()) {
			ath_reg_rmw_clear(ATH_AUD_DPLL2_ADDRESS, ATH_AUD_DPLL2_RANGE_SET(1));
			ath_reg_rmw_set(ATH_AUD_DPLL2_ADDRESS, ATH_AUD_DPLL2_RANGE_SET(1));
		}
		ath_reg_rmw_clear(ATH_AUDIO_PLL_CONFIG, ATH_AUDIO_PLL_CFG_PWR_DWN);

		ath_reg_rmw_clear(ATH_AUD_DPLL3_ADDRESS, ATH_AUD_DPLL3_DO_MEAS_SET(1));
		udelay(100);
		ath_reg_rmw_set(ATH_AUD_DPLL3_ADDRESS, ATH_AUD_DPLL3_DO_MEAS_SET(1));
		udelay(100);

		while ((ath_reg_rd(ATH_AUD_DPLL4_ADDRESS) & ATH_AUD_DPLL4_MEAS_DONE_SET(1)) == 0) {
			udelay(10);
		}
		udelay(100);

		i ++;

	} while (ATH_AUD_DPLL3_SQSUM_DVC_GET(ath_reg_rd(ATH_AUD_DPLL3_ADDRESS)) >= 0x40000);

	printk("\tAud:	0x%x 0x%x\n", KSEG1ADDR(ATH_AUD_DPLL3_ADDRESS),
			ATH_AUD_DPLL3_SQSUM_DVC_GET(ath_reg_rd(ATH_AUD_DPLL3_ADDRESS)));


}


int ath_slic_set_freq(void)
{

		if (ath_ref_freq == (25 * 1000000)) {
			ath_slic_clk(0x127bb02e, 0x61a1);
			ath_slic_dpll(ATH_AUD_DPLL3_KD_25, ATH_AUD_DPLL3_KI_25);
		} else {
			ath_slic_clk(0xfb7e83a, 0x61a2);
			ath_slic_dpll(ATH_AUD_DPLL3_KD_40, ATH_AUD_DPLL3_KI_40);
		}

	return 0;
}

int si3217x_reset(int status)
{
	unsigned int rddata;
#ifdef B6_V3 //B6_V3
	ath_reg_rmw_clear(ATH_GPIO_OE, 1<<19);

	rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION4);
	rddata = rddata & 0xffffff;
//	rddata = rddata | ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_15(0x00);
	ath_reg_wr(ATH_GPIO_OUT_FUNCTION4, rddata);

	if (status) {
		ath_reg_rmw_clear(ATH_GPIO_OUT, 1<<19);
	} else  {
		ath_reg_rmw_set(ATH_GPIO_OUT, 1<<19);
	}
#else
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
#endif
	return 0;
}

int si3217x_init(void){
#if 0
	int retval;
	si3000_reg_t si3000_reg_init_data[] = {
	{0xffff,0x01,0x18},
	{0xffff,0x01,0x18},
	{0xffff,0x02,0x00},
	{0xffff,0x02,0x00},
	{0xffff,0x03,0x00},
	{0xffff,0x03,0x00},
	{0xffff,0x04,0x13},
	{0xffff,0x04,0x13},
	{0xffff,0x05,0x7b},
	{0xffff,0x05,0x7b},
	{0xffff,0x06,0x5c},
	{0xffff,0x06,0x5c},
	{0xffff,0x07,0x5f},
	{0xffff,0x07,0x5f},
	{0xffff,0x08,0x00},
	{0xffff,0x08,0x00},
	{0xffff,0x09,0x00},
	{0xffff,0x09,0x00}};

	retval = si3000_reg_write((char *)si3000_reg_init_data, sizeof(si3000_reg_init_data));
	return retval;
#endif
	return 0;
}

/* Allocate MBOX Buffer descriptors and buffers and
 * initialize them */
int ath_slic_init(int mode)
{
	ath_slic_softc_t *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf;
	slic_buf_t *scbuf;
	uint8_t *bufp = NULL;
	unsigned int sram_desc_start;
	int j, byte_cnt, tail = 0, dir;
	ath_mbox_dma_desc *desc;
	unsigned long desc_p;
	unsigned int icntr = 0, fpcnt = 0;

	if (mode & SLIC_RX) {
		dmabuf = &sc->sc_rbuf;
		dir = DMA_FROM_DEVICE;
	} else {
		dmabuf = &sc->sc_pbuf;
		dir = DMA_TO_DEVICE;
	}

	/* Added since there is a constraint that the total bytes
	 * has to be a integral multiple of num of slots and 32 
	 * Refer: EV 102847 */
//	ath_slic_buf_size = (((SLIC_BUF_SIZE)/(32 * ath_slic_slots_en)) * (32 * ath_slic_slots_en));
	ath_slic_buf_size = SLIC_BUF_SIZE;


	dmabuf->db_desc = (ath_mbox_dma_desc *)
		dma_alloc_coherent(NULL,
				NUM_DESC *
				sizeof(ath_mbox_dma_desc),
				&dmabuf->db_desc_p, GFP_DMA);

	if (dmabuf->db_desc == NULL) {
		printk(KERN_CRIT "DMA desc alloc failed for %d\n",
				mode);
		return ENOMEM;
	}

	for (j = 0; j < NUM_DESC; j++) {
		dmabuf->db_desc[j].OWN = 0;
	}

	/* Allocate Data Buffers */
	scbuf = dmabuf->db_buf;
	if (!(bufp = kmalloc(NUM_DESC * ath_slic_buf_size, GFP_KERNEL | GFP_DMA))) {
		printk(KERN_CRIT
				"Buffer allocation failed for \n");
		goto fail3;
	}
	
	if (mode & SLIC_RX) {
		/* Initialize the receive memory to 0xa5 */
		memset(bufp, 0/*0xa5*/, (NUM_DESC*ath_slic_buf_size));
		sc->sc_rmall_buf = bufp;
	} else {
//		for(fpcnt=0; fpcnt<(NUM_DESC*ath_slic_buf_size); fpcnt++) {
			/* Fill buffer pattern 0x11, 0x22 ..... For ex,
			 * if slots_en = 3, Tx buffer is filled as 0x11,
                         * 0x22, 0x33.*/
//			icntr += 1 ; 
//			*(bufp+fpcnt) = icntr  | ( icntr << 4 ) ;
//			if (icntr == ath_slic_slots_en) {
//				icntr = 0;
//			}
//		}
		memset(bufp, 0, (NUM_DESC*ath_slic_buf_size));
		sc->sc_pmall_buf = bufp;
	}

	for (j = 0; j < NUM_DESC; j++) {
		scbuf[j].bf_vaddr = &bufp[j * ath_slic_buf_size];
#ifdef USE_SRAM
		scbuf[j].bf_paddr = scbuf[j].bf_vaddr ;
#else
		scbuf[j].bf_paddr =
			dma_map_single(NULL, scbuf[j].bf_vaddr, ath_slic_buf_size, dir);
#endif
	}
	dmabuf->tail = 0;

	// Initialize Desc
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	byte_cnt = NUM_DESC * ath_slic_buf_size;
	tail = dmabuf->tail;

	while (byte_cnt && (tail < NUM_DESC)) {
		desc[tail].rsvd1 = 0;
		desc[tail].size = ath_slic_buf_size;
		if (byte_cnt > ath_slic_buf_size) {
			desc[tail].length = ath_slic_buf_size;
			byte_cnt -= ath_slic_buf_size;
			desc[tail].EOM = 0;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
			desc[tail].EOM = 0;
		}
		desc[tail].rsvd2 = 0;
		desc[tail].rsvd3 = 0;
		desc[tail].BufPtr =
			(unsigned int) scbuf[tail].bf_paddr;
		desc[tail].NextPtr =
			(desc_p +
			 ((tail +
			   1) *
			  (sizeof(ath_mbox_dma_desc))));
		if (mode & SLIC_RX) {
			desc[tail].OWN = 1;
		} else {
			desc[tail].OWN = 0;
		}
		tail++;
	}
	tail--;
	desc[tail].NextPtr = desc_p;

	dmabuf->tail = 0;
	dmabuf->play_tail = 0;

	if (mode & SLIC_RX) {
		printk("Tx Desc Base 0x%08lx\n", desc_p);
	} else {
		printk("Rx Desc Base 0x%08lx\n", desc_p);
	}
	return 0;

fail3:
	if (mode & SLIC_RX)
		dmabuf = &sc->sc_rbuf;
	else
		dmabuf = &sc->sc_pbuf;
#ifndef USE_SRAM
	/* Free up memories only if USE_SRAM is not defined */
	dma_free_coherent(NULL,
			  NUM_DESC * sizeof(ath_mbox_dma_desc),
			  dmabuf->db_desc, dmabuf->db_desc_p);

	if (mode & SLIC_RX) {
		if (sc->sc_rmall_buf)
			kfree(sc->sc_rmall_buf);
	} else {
		if (sc->sc_pmall_buf)
			kfree(sc->sc_pmall_buf);
	}

#endif
	return -ENOMEM;

}

int ath_slic_open_mode(int mode)
{

	ath_slic_softc_t *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf;
	ath_mbox_dma_desc *desc;
	int j;

	if (mode & SLIC_RX) {
		dmabuf = &sc->sc_rbuf;
		sc->ropened = 1;
		sc->rpause = 0;
		memset(sc->sc_rmall_buf, 0/*0x5a*/, (NUM_DESC*ath_slic_buf_size));
	} else {
		dmabuf = &sc->sc_pbuf;
		sc->popened = 1;
		sc->ppause = 0;
		memset(sc->sc_pmall_buf, 0, (NUM_DESC*ath_slic_buf_size));
	}

	desc = dmabuf->db_desc;
	dmabuf->play_tail = 0;
	dmabuf->tail = 0;

//	for (j = 0; j < NUM_DESC; j++) {
//		if (mode & SLIC_RX){
//			desc[j].OWN = 1;
//		} else {
// 			desc[j].OWN = 0;
//		}
//	}

	return (0);

}

int ath_slic_open(struct inode *inode, struct file *filp)
{

	ath_slic_softc_t *sc = &sc_buf_var;
	int opened = 0, j;
	slic_dma_buf_t *dmabuf;
	ath_mbox_dma_desc *desc;
	slic_buf_t *scbuf;
	unsigned long desc_p;
	printk(KERN_CRIT "ath_slic_open ...\n");
#if 1
	if ((filp->f_mode & FMODE_READ) && (sc->ropened)) {
		printk("%s, %d SLIC Rx busy\n", __func__, __LINE__);
		return -EBUSY;
	}
	if ((filp->f_mode & FMODE_WRITE) && (sc->popened)) {
		printk("%s, %d SLIC Tx busy\n", __func__, __LINE__);
		return -EBUSY;
	}
#endif
//	opened = (sc->ropened | sc->popened);

	//初始化SLIC
	if(!slic_dma_open)
	{
		//清空寄存器 关时钟
		ath_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_SLIC_RESET_SET(1));
		ath_reg_wr(ATH_SLIC_CTRL, ATH_SLIC_CTRL_MASTER);
//		mdelay(1);
		
		dmabuf = &sc->sc_rbuf;
		desc = dmabuf->db_desc;
		dmabuf->play_tail = dmabuf->tail = 0;
		for (j = 0; j < NUM_DESC; j++) {
			desc[j].OWN = 1;
		}
		
		dmabuf = &sc->sc_pbuf;
		desc = dmabuf->db_desc;
		dmabuf->play_tail = dmabuf->tail = 0;
		for (j = 0; j < NUM_DESC; j++) {
 			desc[j].OWN = 1;
		}
		//使能中断
		ath_reg_rmw_set(ATH_MBOX_SLIC_INT_ENABLE, 
					( ATH_MBOX_SLIC_TX_DMA_COMPLETE ));
		ath_reg_rmw_set(ATH_MBOX_SLIC_INT_ENABLE, 
					( ATH_MBOX_SLIC_RX_DMA_COMPLETE ));
		mdelay(1);
	}
	slic_dma_open = 1;
	if(filp->f_mode & FMODE_READ)
	{
		ath_slic_open_mode(SLIC_RX);
		dmabuf = &sc->sc_rbuf;
		scbuf = dmabuf->db_buf;
		desc_p = (unsigned long) dmabuf->db_desc_p;
		memset(sc->sc_rmall_buf, 0, NUM_DESC * ath_slic_buf_size);
		for (j = 0; j < NUM_DESC; j++) {
 			dma_cache_sync(NULL, scbuf[j].bf_vaddr, ath_slic_buf_size, DMA_FROM_DEVICE);
		}
		ath_slic_dma_start(desc_p, 1);
//		printk("Slic Start sc_rbuf DMA\n");
	}
	if(filp->f_mode & FMODE_WRITE)
	{
		ath_slic_open_mode(SLIC_TX);
		dmabuf = &sc->sc_pbuf;
		scbuf = dmabuf->db_buf;
		memset(&sc_pbuf_flag, 0, sizeof(sc_pbuf_flag));
		memset(sc->sc_pmall_buf, 0, NUM_DESC * ath_slic_buf_size);
		for (j = 0; j < NUM_DESC; j++) {
 			dma_cache_sync(NULL, scbuf[j].bf_vaddr, ath_slic_buf_size, DMA_TO_DEVICE);
		}
		desc_p = (unsigned long) dmabuf->db_desc_p;
		ath_slic_dma_start(desc_p, 0);
//		printk("Slic Start sc_pbuf DMA\n");
	}
	
	do_echo_filter = 0;
	return (0);

}

/* Receive */
ssize_t ath_slic_read(struct file * filp, char __user * buf,
			 size_t count, loff_t * f_pos)
{
	uint8_t *data;
	ssize_t retval;
	int byte_cnt, offset = 0, need_start = 0;
	int mode = 1, index = 0;
	struct ath_slic_softc *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf = &sc->sc_rbuf;
	slic_buf_t *scbuf;
	ath_mbox_dma_desc *desc;
	int tail = dmabuf->tail;
//	int play_tail = dmabuf->play_tail;
	unsigned long desc_p;
	
#ifdef DO_AUDIO_FILTER
	int postion_r;
	int postion_w;
	slic_dma_buf_t *pl_dmabuf = &sc->sc_pbuf;
	slic_dma_buf_t *rl_dmabuf = &sc->sc_rbuf;
	slic_buf_t *pl_scbuf = pl_dmabuf->db_buf;
#endif
	
	byte_cnt = count;


/*	if (sc->ropened < 2) {
		ath_reg_rmw_set(ATH_MBOX_SLIC_INT_ENABLE, 
				( ATH_MBOX_SLIC_TX_DMA_COMPLETE ));
		need_start = 1;
	}*/

	sc->ropened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
//	data = scbuf[0].bf_vaddr;

//	desc_p += tail * sizeof(ath_mbox_dma_desc);
	
/*	if (!need_start) {
//		wait_event_interruptible(wq_tx, desc[tail].OWN != 1);
		wait_event_interruptible(wq_tx, (play_tail != dmabuf->tail));

	}*/

//	while (byte_cnt && !desc[tail].OWN) {
	while(byte_cnt && (tail != dmabuf->play_tail)){
		desc[tail].rsvd1 = 0;
		desc[tail].size = ath_slic_buf_size;
		if (byte_cnt >= ath_slic_buf_size) {
			desc[tail].length = ath_slic_buf_size;
			byte_cnt -= ath_slic_buf_size;
			desc[tail].EOM = 0;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
			desc[tail].EOM = 0;
		}

			dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_FROM_DEVICE);

			desc[tail].rsvd2 = 0;
#if 0
			for(index = 0; index < desc[tail].length; index = index + 2){	
				le16_to_cpus(scbuf[tail].bf_vaddr + index); 
				} 
#endif
//进行侧音处理
#ifdef DO_AUDIO_FILTER

		//按照条件执行回声抑制处理
		if(do_echo_filter)
		{
			postion_r = prev_tail(rl_dmabuf->play_tail);
			postion_r = space_valid(postion_r, tail);
			
			postion_w = prev_tail(pl_dmabuf->play_tail);
			postion_w = space_valid(postion_w, postion_r);
			
			AudioDriverReadFilterDo(pl_scbuf[postion_w].bf_vaddr,scbuf[tail].bf_vaddr);
		}
#endif
			retval = copy_to_user(buf + offset, scbuf[tail].bf_vaddr, desc[tail].length);
//			printk("copy_to_user....\n");
			if (retval)
				return retval;

		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;

		desc[tail].rsvd3 = 0;
		/* Give ownership back to MBOX. */
		desc[tail].OWN = 1;

		offset += desc[tail].length;
		tail = next_tail(tail);
	}

	dmabuf->tail = tail;
//	dmabuf->play_tail = play_tail;

//	if (need_start) {
		/* Start DMA */
//		printk("TX DMA Start....\n");
//		ath_slic_dma_start((unsigned long) desc_p, mode);
//	} else if (!sc->rpause) {
//		ath_slic_dma_resume(mode);
//	}
	return offset;
}

/* Transmit */
ssize_t ath_slic_write(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos)
{
	uint8_t *data;
	ssize_t retval;
	int byte_cnt, offset = 0, need_start = 0;
	int mode = 0,index = 0;
	struct ath_slic_softc *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf = &sc->sc_pbuf;
	slic_buf_t *scbuf;
	ath_mbox_dma_desc *desc;
	int tail = dmabuf->tail;
//	unsigned long desc_p;
	int block_size=0;
//	si3000 = 0;
	byte_cnt = count;
//	unsigned short test;

/*	if (sc->popened < 2) {
		ath_reg_rmw_set(ATH_MBOX_SLIC_INT_ENABLE, 
				( ATH_MBOX_SLIC_RX_DMA_COMPLETE ));
		need_start = 1;
	}

	sc->popened = 2;
*/
	//判断当前是否是无效数据
	while(space_valid(tail, dmabuf->play_tail) > 1)
	{
		if(sc_pbuf_flag.user_add[tail] == 0)
		{
			tail = prev_tail(tail);
		}
		else
		{
			dmabuf->tail = tail;
			break;
		}
	}
	
	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
//	desc_p = (unsigned long) dmabuf->db_desc_p;
	offset = 0;
	data = scbuf[0].bf_vaddr;

//	desc_p += tail * sizeof(ath_mbox_dma_desc);
	
	/* Send count bytes of data */
//	while (byte_cnt && !desc[tail].OWN) {
	while(byte_cnt){
		//空闲空间太小则返回
		if(dmabuf->play_tail == tail)
			;
		else if(space_valid(dmabuf->play_tail, tail) < (NUM_DESC/4))
			break;
//		if(space_valid(tail, dmabuf->play_tail) > (NUM_DESC*4/5))
//			break;
		desc[tail].rsvd1 = 0;
		desc[tail].size = ath_slic_buf_size;
		if (byte_cnt >= ath_slic_buf_size) {
			desc[tail].length = ath_slic_buf_size;
			byte_cnt -= ath_slic_buf_size;
			desc[tail].EOM = 0;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
			desc[tail].EOM = 0;
		}

        retval = copy_from_user(scbuf[tail].bf_vaddr, buf + offset, desc[tail].length);
//		printk("copy_from_user = %d \n",desc[tail].length);
        if (retval)
            return retval;
#if 0
		for(index = 0; index < desc[tail].length; index = index + 2){
			cpu_to_le16s(scbuf[tail].bf_vaddr + index);
		} 
#endif
		dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_TO_DEVICE);

		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
	
		desc[tail].rsvd2 = 0;
		desc[tail].rsvd3 = 0;
		desc[tail].OWN = 1;

		sc_pbuf_flag.user_add[tail] = 1;
		offset += desc[tail].length;
		
		tail = next_tail(tail);
		dmabuf->tail = tail;
	}

/*	if (need_start) {
		ath_slic_dma_start((unsigned long) desc_p, mode);
	} else if (!sc->ppause) {
		if (is_ar934x()) {
			if ((ath_reg_rd(ATH_MBOX_SLIC_FRAME) & ATH_MBOX_SLIC_FRAME_RX_SOM) == ATH_MBOX_SLIC_FRAME_RX_SOM) {
				if (ath_slic_slave) {
					ath_reg_wr(ATH_SLIC_CTRL, (ATH_SLIC_CTRL_CLK_EN));
				} else {
					ath_reg_wr(ATH_SLIC_CTRL, (ATH_SLIC_CTRL_CLK_EN | ATH_SLIC_CTRL_MASTER));
				}

				ath_slic_dma_resume(mode);
				udelay(100);
				ath_reg_rmw_set(ATH_SLIC_CTRL, ATH_SLIC_CTRL_EN);
			} else {
				ath_slic_dma_resume(mode);
			}
		} else {
			ath_slic_dma_resume(mode);
		}
	}*/
	return count - byte_cnt;
}




int ath_slic_close_mode(int mode)
{
	int j, own;

	ath_slic_softc_t *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf;
	ath_mbox_dma_desc *desc;

	if (mode & SLIC_RX) {
		dmabuf = &sc->sc_rbuf;
		own = sc->rpause;
//		mode = 1;
	} else {
		dmabuf = &sc->sc_pbuf;
		own = sc->ppause;
//		mode = 0;
	}

	desc = dmabuf->db_desc;
	for (j = 0; j < NUM_DESC; j++) {
		desc[j].OWN = 0;
	}

	dmabuf->tail = 0;

	if (mode & SLIC_RX) {
		sc->ropened = 0;
		sc->rpause = 0;
	} else {
		sc->popened = 0;
		sc->ppause = 0;
	}


	return (0);
}


int ath_slic_close(struct inode *inode, struct file *filp)
{
	int j;
	ath_slic_softc_t *sc = &sc_buf_var;
//	printk(KERN_CRIT "ath_slic_close ...\n");
	slic_dma_buf_t *dmabuf_r, *dmabuf_p;

	dmabuf_r = &sc->sc_rbuf;
	dmabuf_p = &sc->sc_pbuf;

	if(filp->f_mode & FMODE_READ)
	{
		//清中断
//		ath_reg_rmw_clear(ATH_MBOX_SLIC_INT_ENABLE, 
//				( ATH_MBOX_SLIC_TX_DMA_COMPLETE ));
		ath_slic_close_mode(SLIC_RX);
/*
		for (j = 0; j < NUM_DESC; j++) {
		dma_unmap_single(NULL, dmabuf_r->db_buf[j].bf_paddr,
				ath_slic_buf_size, DMA_BIDIRECTIONAL);
		}
		kfree(sc->sc_rmall_buf);
		dma_free_coherent(NULL,
			NUM_DESC * sizeof(ath_mbox_dma_desc),
			dmabuf_r->db_desc, dmabuf_r->db_desc_p);
*/
//		printk("ath_slic_close read ...\n");
	}
	if(filp->f_mode & FMODE_WRITE)
	{
//		ath_reg_rmw_clear(ATH_MBOX_SLIC_INT_ENABLE, 
//				( ATH_MBOX_SLIC_RX_DMA_COMPLETE ));
		ath_slic_close_mode(SLIC_TX);
/*
		dma_unmap_single(NULL, dmabuf_p->db_buf[j].bf_paddr,
				ath_slic_buf_size, DMA_BIDIRECTIONAL);
		kfree(sc->sc_pmall_buf);
		dma_free_coherent(NULL,
			NUM_DESC * sizeof(ath_mbox_dma_desc),
			dmabuf_p->db_desc, dmabuf_p->db_desc_p);
*/
//		printk("ath_slic_close write ...\n");
	}
	//关闭时钟
	if((sc->ropened == 0) &&(sc->popened == 0))
	{
		//清空寄存器 关时钟
//		ath_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_SLIC_RESET_SET(1));
//		ath_reg_wr(ATH_SLIC_CTRL, ATH_SLIC_CTRL_MASTER);
		printk("slic both close over\n");
		slic_dma_open = 0;
	}
	
	return (0);
}

int ath_slic_release(struct inode *inode, struct file *filp)
{
	printk(KERN_CRIT "release\n");
	return 0;
}

int ath_slic_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	int data;
	switch(cmd) {
		case SLIC_BITSWAP:
			data = arg;
			/* Rx bit swap */
			if (data) {
				ath_reg_rmw_set(ATH_MBOX_SLIC_DMA_POLICY,
						MBOX_DMA_POLICY_RXD_16BIT_SWAP_SET(1));
			}
			else {
				/* Tx Bit Swap */
				ath_reg_rmw_set(ATH_MBOX_SLIC_DMA_POLICY,
					MBOX_DMA_POLICY_TXD_16BIT_SWAP_SET(1));
			}
		break;
		case SLIC_BYTESWAP:
			data = arg;
			/* Rx byte swap */
			if (data) {
				ath_reg_rmw_set(ATH_MBOX_SLIC_DMA_POLICY,
					MBOX_DMA_POLICY_RXD_END_SWAP_SET(1));
			}
			else {
				/* Tx Byte Swap */
				ath_reg_rmw_set(ATH_MBOX_SLIC_DMA_POLICY,
					MBOX_DMA_POLICY_TXD_END_SWAP_SET(1));
			}
		break;
		case SET_WRITE_TYPE:
			data = arg;
			if(data)
			{
				do_echo_filter = 1;
//				printk("Set Echo Filter Flag*****\n");
			}			
			else
				do_echo_filter = 0;
			break;
		case SLIC_INIT:
			{
				ath_slic_softc_t *sc = &sc_buf_var;
				slic_dma_buf_t *dmabuf;
//				ath_mbox_dma_desc *desc;
				printk("Slic Init...\n");
				//清空读写缓存				
				dmabuf = &sc->sc_rbuf;
				dmabuf->tail = dmabuf->play_tail;
				dmabuf = &sc->sc_pbuf;
				dmabuf->tail = next_tail(dmabuf->play_tail);
				//清空写缓存
//				memset(sc->sc_pmall_buf, 0, NUM_DESC * ath_slic_buf_size);
				do_echo_filter = 0;
				memset(&sc_pbuf_flag, 0, sizeof(sc_pbuf_flag));
				slic_dma_open = 1;
			}
			break;
		case SLIC_RELEASE:
			{
				int j;
				ath_slic_softc_t *sc = &sc_buf_var;
				slic_dma_buf_t *dmabuf;
				slic_buf_t *scbuf;;
				dmabuf = &sc->sc_pbuf;
				scbuf = dmabuf->db_buf;
				slic_dma_open = 0;
				//清空写缓存
				memset(sc->sc_pmall_buf, 0, NUM_DESC * ath_slic_buf_size);
				for (j = 0; j < NUM_DESC; j++) {
					dma_cache_sync(NULL, scbuf[j].bf_vaddr, ath_slic_buf_size, DMA_TO_DEVICE);
				}
				printk("Slic release...\n");
			}
			break;
		default:
			return -ENOTSUPP;
		}
	return 0;
}


irqreturn_t ath_slic_intr(int irq, void *dev_id)
{
	unsigned int int_status;
	struct ath_slic_softc *sc = &sc_buf_var;
//	slic_dma_buf_t *dmabuf = &sc->sc_rbuf;
//	ath_mbox_dma_desc *desc = dmabuf->db_desc;
//	int tail = dmabuf->tail;
	slic_dma_buf_t *dmabuf;
	ath_mbox_dma_desc *desc;
	slic_buf_t *scbuf;
//	int play_tail = dmabuf->play_tail;
	int_status = ath_reg_rd(ATH_MBOX_SLIC_INT_STATUS);

	if (int_status & ATH_MBOX_SLIC_RX_DMA_COMPLETE) {
//		wake_up_interruptible(&wq_rx);
//		printk("ath_slic: ATH_MBOX_SLIC_RX_DMA_COMPLETE....\n");
		dmabuf = &sc->sc_pbuf;
		desc = dmabuf->db_desc;
		//把当前处理 DMA还原
		desc[dmabuf->play_tail].OWN = 1;
		
		//
		if(slic_dma_open)
		{
			//如果后续第二包没有数据，则用0补充
			if(space_valid(dmabuf->tail, dmabuf->play_tail) <= 1)
			{
				scbuf = dmabuf->db_buf;
//				desc = dmabuf->db_desc;
				
//				desc[dmabuf->tail].rsvd1 = 0;
//				desc[dmabuf->tail].size = ath_slic_buf_size;
//				desc[dmabuf->tail].EOM = 0;
				desc[dmabuf->tail].length = ath_slic_buf_size;
				
				memset(scbuf[dmabuf->tail].bf_vaddr, 0, ath_slic_buf_size);
				dma_cache_sync(NULL, scbuf[dmabuf->tail].bf_vaddr, desc[dmabuf->tail].length, DMA_TO_DEVICE);
	
//				desc[dmabuf->tail].BufPtr = (unsigned int) scbuf[dmabuf->tail].bf_paddr;
		
//				desc[dmabuf->tail].rsvd2 = 0;
//				desc[dmabuf->tail].rsvd3 = 0;
				desc[dmabuf->tail].OWN = 1;
				//置数据来源标志位0
				sc_pbuf_flag.user_add[dmabuf->tail] = 0;
				
				dmabuf->tail = next_tail(dmabuf->tail);
//				printk("ath_slic_intr add one block\n");
			}
		}
		dmabuf->play_tail = next_tail(dmabuf->play_tail);
//		ath_slic_dma_resume(0);
	}
	
	if (int_status & ATH_MBOX_SLIC_TX_DMA_COMPLETE) {
//		wake_up_interruptible(&wq_tx);
//		printk("ath_slic: ATH_MBOX_SLIC_TX_DMA_COMPLETE....%d\n", dmabuf->tail);
		dmabuf = &sc->sc_rbuf;
//		scbuf = dmabuf->db_buf;
		desc = dmabuf->db_desc;
		//置标志
//		desc[dmabuf->play_tail].size = ath_slic_buf_size;
//		desc[dmabuf->play_tail].length = ath_slic_buf_size;
//		desc[dmabuf->play_tail].EOM = 0;
//		desc[dmabuf->play_tail].rsvd1 = 0;
//		desc[dmabuf->play_tail].rsvd2 = 0;
//		desc[dmabuf->play_tail].rsvd3 = 0;
//		desc[dmabuf->play_tail].BufPtr = (unsigned int) scbuf[dmabuf->play_tail].bf_paddr;
		desc[dmabuf->play_tail].OWN = 1;
		
		dmabuf->play_tail = next_tail(dmabuf->play_tail);
//		ath_slic_dma_resume(1);
	}

	if (int_status & ATH_MBOX_SLIC_RX_UNDERFLOW) {
		printk("ath_slic: Underflow Encountered....\n");
	}
	if (int_status & ATH_MBOX_SLIC_TX_OVERFLOW) {
		printk("ath_slic: Overflow Encountered....\n");
	}

	/* Ack the interrupts */
//	if (is_qca955x()) {
	if (0) {
		ath_reg_wr(ATH_MBOX_SLIC_INT_STATUS, 
				int_status & (~(ATH_MBOX_SLIC_RX_DMA_COMPLETE | ATH_MBOX_SLIC_TX_DMA_COMPLETE | 
						ATH_MBOX_SLIC_TX_OVERFLOW | ATH_MBOX_SLIC_RX_UNDERFLOW)));
	} else {
		ath_reg_wr(ATH_MBOX_SLIC_INT_STATUS, int_status);
	}
	return IRQ_HANDLED;
}

void ath_slic_enable(void)
{
//	int slot = 0, slot2 = 0;
	ath_reset(ATH_RESET_SLIC);    // SLIC reset
	udelay(1000);
#if 1
	ath_reg_wr(ATH_STEREO_CONFIG, (ATH_STEREO_CONFIG_SPDIF_ENABLE           |
				ATH_STEREO_CONFIG_ENABLE                 |
				ATH_STEREO_CONFIG_RESET                  |
				ATH_STEREO_CONFIG_DATA_WORD_SIZE(1)      |
				ATH_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE  |
				ATH_STEREO_CONFIG_MASTER));
#else
	ath_reg_wr(ATH_STEREO_CONFIG, (ATH_STEREO_CONFIG_ENABLE	|
				ATH_STEREO_CONFIG_RESET                  |
				ATH_STEREO_CONFIG_DATA_WORD_SIZE(0)      |
				ATH_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE  |
				ATH_STEREO_CONFIG_MASTER));
#endif
	/* Program TIMING CTRL register */
	if (ath_slic_slave) {
		ath_reg_wr(ATH_SLIC_CLOCK_CTRL, ATH_SLIC_CLOCK_CTRL_DIV(0x0));
//		ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND(1) |
		ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND |
					ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS(1)     |
					ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(1)        |
					ATH_SLIC_TIMING_CTRL_FS_POS | ATH_SLIC_TIMING_CTRL_LONG_FS));
	} else {
		ath_reg_wr(ATH_SLIC_CLOCK_CTRL, ATH_SLIC_CLOCK_CTRL_DIV(0x05)); // earlier value 0xf
//		ath_reg_wr(ATH_SLIC_CLOCK_CTRL, ATH_SLIC_CLOCK_CTRL_DIV(0x17)); // earlier value 0xf
#if 0
		ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND    |
					ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS(1)     |
					ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(2) |
					ATH_SLIC_TIMING_CTRL_FS_POS | ATH_SLIC_TIMING_CTRL_LONG_FS));
#else
		ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS(1)	  |
				ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(2) |
				ATH_SLIC_TIMING_CTRL_FS_POS | ATH_SLIC_TIMING_CTRL_LONG_FS));
#endif
	}

	if (!ath_slic_long_fs) {
		ath_reg_rmw_clear(ATH_SLIC_TIMING_CTRL, ATH_SLIC_TIMING_CTRL_LONG_FS);
	}
	else {
		ath_reg_rmw_set(ATH_SLIC_TIMING_CTRL, ATH_SLIC_TIMING_CTRL_LONG_FS);
	}
	ath_reg_wr(ATH_SLIC_SLOT, ATH_SLIC_SLOT_SEL(ath_slic_max_slots));
	
	/* Finding the value filled in to Tx/Rx slot registers based
           on slots_en. If slots_en=8, the value to be filled in is 
           0xff, if slots_en=2, value to be filled in=3 */
#if 0
	if (ath_slic_slots_en < MAX_REG_BITS)
		slot = ((1 << ath_slic_slots_en) - 1);
	else {
		slot = SLIC_SLOTS_EN_ALL;
		slot2 = ((1 << (ath_slic_slots_en - MAX_REG_BITS)) - 1);
	}
  
	if (ath_slic_wide) {
		/* Wide Band */
		ath_reg_wr(ATH_SLIC_TX_SLOTS1, ATH_SLIC_TX_SLOTS1_EN(slot));
		ath_reg_wr(ATH_SLIC_RX_SLOTS1, ATH_SLIC_RX_SLOTS1_EN(slot));
	} else {
		/* Narrow Band */
		/* Fill up Tx/Rx slot en. */
		ath_reg_wr(ATH_SLIC_TX_SLOTS1, ATH_SLIC_TX_SLOTS1_EN(slot));
		ath_reg_wr(ATH_SLIC_RX_SLOTS1, ATH_SLIC_RX_SLOTS1_EN(slot));

		ath_reg_wr(ATH_SLIC_TX_SLOTS2, ATH_SLIC_TX_SLOTS2_EN(slot2));
		ath_reg_wr(ATH_SLIC_RX_SLOTS2, ATH_SLIC_RX_SLOTS2_EN(slot2));
 	}
#endif

		ath_reg_wr(ATH_SLIC_TX_SLOTS1, ATH_SLIC_TX_SLOTS1_EN(0x3));
		ath_reg_wr(ATH_SLIC_RX_SLOTS1, ATH_SLIC_RX_SLOTS1_EN(0x3));

//	ath_reg_rmw_set(ATH_SLIC_SWAP, ATH_SLIC_SWAP_RX_DATA);
	
	/* Enable SLIC in master/slave mode */
	if (ath_slic_slave) {
		ath_reg_wr(ATH_SLIC_CTRL, (ATH_SLIC_CTRL_CLK_EN));
	} else {
		ath_reg_wr(ATH_SLIC_CTRL, (ATH_SLIC_CTRL_CLK_EN | ATH_SLIC_CTRL_MASTER));
	}
	ath_reg_rmw_set(ATH_SLIC_CTRL, ATH_SLIC_CTRL_EN);
}


/* The initialization sequence carries hardcoded values.
 * Register descriptions are to be defined and updated.
 * TODO list.
 */
void ath_slic_link_on(void)
{
	unsigned int rddata = 0;
//	int slot = 0, slot2 = 0;
	ath_reset(ATH_RESET_SLIC);    // SLIC reset
	udelay(1000);
//	if (is_qca955x()) {
	if (0) {
		/*
		 * To Be Done
		 * Add SLIC MBOX Soft Reset, Reg ATH_MBOX_SLIC_DMA_POLICY Bit 31 - Set and Clear.
		 */
#ifdef CONFIG_ATH_EMULATION
		 // SLIC DI is an input on GPIO12
		rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE4) & ~GPIO_IN_ENABLE4_SLIC_DATA_IN_MASK) | GPIO_IN_ENABLE4_SLIC_DATA_IN_SET(12);
		ath_reg_wr(ATH_GPIO_IN_ENABLE4, rddata);

		if (ath_slic_slave) {
			// SLIC MCLK is input on GPIO15 as I2S EXT MCLK
			rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE2) & ~ATH_GPIO_IN_ENABLE1_I2SEXT_MCLK_MASK) | ATH_GPIO_IN_ENABLE1_I2SEXT_MCLK(13);
			ath_reg_wr(ATH_GPIO_IN_ENABLE2, rddata);
		} else {
			  // SLIC MCLK is an output on GPIO15 and has a function index 5
			rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION3) | GPIO_OUT_FUNCTION3_ENABLE_GPIO_13_SET(5);
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);
		}
			// SLIC Data Out is on GPIO 17 and has a function index of 3
			rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION4) | GPIO_OUT_FUNCTION4_ENABLE_GPIO_17_SET(3);
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION4, rddata);
		
		if (ath_slic_slave) {
			// SLIC FS is input on GPIO 11
			rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE4) & ~GPIO_IN_ENABLE4_SLIC_PCM_FS_IN_MASK) | GPIO_IN_ENABLE4_SLIC_PCM_FS_IN_SET(11);
			ath_reg_wr(ATH_GPIO_IN_ENABLE4, rddata);
		} else {
		// SLIC FS is on GPIO 11 and has function index 4
			rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION2) | GPIO_OUT_FUNCTION2_ENABLE_GPIO_11_SET(4);
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION2, rddata);
		}

		rddata = ath_reg_rd(ATH_GPIO_OE); //0x2f30b

		if (ath_slic_slave) {
			rddata = rddata & ~(0x20000) ;
		} else {
			rddata = rddata & ~(0x2A800) ;
		}
		ath_reg_wr(ATH_GPIO_OE, rddata);

		// Enable the inputs now
		rddata = ath_reg_rd(ATH_GPIO_OE); //0x2f30b
		
		if (ath_slic_slave) {
			rddata = rddata | (0x9800) ;
		} else {
			rddata = rddata | (0x1000) ;
		}
		ath_reg_wr(ATH_GPIO_OE, rddata);
#else	
		/* SLIC DI is an input on GPIO14 */
		rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE4) & ~GPIO_IN_ENABLE4_SLIC_DATA_IN_MASK) | GPIO_IN_ENABLE4_SLIC_DATA_IN_SET(14);
		ath_reg_wr(ATH_GPIO_IN_ENABLE4, rddata);

		if (ath_slic_slave) {
			/* SLIC MCLK is input on GPIO13 as SLIC EXT MCLK */
			rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE2) & ~GPIO_IN_ENABLE2_SLICEXT_MCLK_MASK) | GPIO_IN_ENABLE2_SLICEXT_MCLK_SET(13);

			ath_reg_wr(ATH_GPIO_IN_ENABLE2, rddata);   
		} else {
			/* SLIC MCLK is an output on GPIO13 and has a function index 5 */
			rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION3) | GPIO_OUT_FUNCTION3_ENABLE_GPIO_13_SET(5);
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);
		}

		/* SLIC Data Out is on GPIO 4 and has a function index of 3 */
		rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION1) | GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_SET(3);
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION1, rddata);

		if (ath_slic_slave) {
			/* SLIC FS is input on GPIO 12 */
			rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE4) & ~GPIO_IN_ENABLE4_SLIC_PCM_FS_IN_MASK) | GPIO_IN_ENABLE4_SLIC_PCM_FS_IN_SET(12);
			ath_reg_wr(ATH_GPIO_IN_ENABLE4, rddata);
		} else {
			/* SLIC FS is on GPIO 12 and has function index 4 */
			rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION3) | GPIO_OUT_FUNCTION3_ENABLE_GPIO_12_SET(4);
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);
		}


		rddata = ath_reg_rd(ATH_GPIO_OE); //0x2f30b
		/* Enable the outputs first */
		if (ath_slic_slave) {
			rddata = rddata & ~(0x0010) ;
		} else {
			rddata = rddata & ~(0x03010) ;
		}
		ath_reg_wr(ATH_GPIO_OE, rddata);

		/* Enable the inputs now */
		rddata = ath_reg_rd(ATH_GPIO_OE); //0x2f30b
		if (ath_slic_slave) {
			rddata = rddata | (0x7000) ;
		} else {
			rddata = rddata | (0x4000) ;
		}
		ath_reg_wr(ATH_GPIO_OE, rddata);

		/*
		 * To Be Done
		 * Program Clock Source (ATH_SLIC_CTRL) Select, PLL and Divider
		 * Initialize SLIC
		 */
		if (ath_slic_slave) {
//			ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND(1) |
			ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND |
                                        ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS(1)     |
                                        ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(1)        |
                                        ATH_SLIC_TIMING_CTRL_FS_POS | ATH_SLIC_TIMING_CTRL_LONG_FS));
			
		} else {
			/* For Loopback testing */
//			ath_reg_wr((0xb8050000 + 0x54), 0xC4000009); // Clock Divider
//                       ath_reg_wr(ATH_SLIC_TIMING_CTRL, 0xC3); 
	
			ath_reg_wr(SLIC_PCM_DIV_ADDRESS, 0xC4000009); // Clock Divider
//			ath_reg_wr(ATH_SLIC_TIMING_CTRL, 0x5a3); 
//			ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND(0) |
			ath_reg_wr(ATH_SLIC_TIMING_CTRL, (ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND |
                                        ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS(3)     |
                                        ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(2)        |
                                        ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC_EXTEND    | 
                                        ATH_SLIC_TIMING_CTRL_FS_POS | ATH_SLIC_TIMING_CTRL_LONG_FS));

		}

		ath_reg_wr(ATH_SLIC_SLOT, ATH_SLIC_SLOT_SEL(ath_slic_max_slots));
#if 0
		/* Find Tx/Rx SLOT En register values based on ath_slic_slot_en */
		if (ath_slic_slots_en < MAX_REG_BITS)
	                slot = ((1 << ath_slic_slots_en) - 1);
	        else {
        	        slot = SLIC_SLOTS_EN_ALL;
                	slot2 = ((1 << (ath_slic_slots_en - MAX_REG_BITS)) - 1);
        	}

		if (ath_slic_wide) {
			/* Wide Band */
			/* ath_reg_wr(ATH_SLIC_TX_SLOTS1, 0xf000f); 
			ath_reg_wr(ATH_SLIC_RX_SLOTS1, 0xf000f);*/
			ath_reg_wr(ATH_SLIC_TX_SLOTS1, ATH_SLIC_TX_SLOTS1_EN(slot));
			ath_reg_wr(ATH_SLIC_RX_SLOTS1, ATH_SLIC_RX_SLOTS1_EN(slot));

		} else {
			/* Narrow Band */
			ath_reg_wr(ATH_SLIC_TX_SLOTS1, ATH_SLIC_TX_SLOTS1_EN(slot));
			ath_reg_wr(ATH_SLIC_TX_SLOTS2, ATH_SLIC_TX_SLOTS2_EN(slot2));
			ath_reg_wr(ATH_SLIC_RX_SLOTS1, ATH_SLIC_RX_SLOTS1_EN(slot));
			ath_reg_wr(ATH_SLIC_RX_SLOTS2, ATH_SLIC_RX_SLOTS2_EN(slot2));
		}
#endif
		ath_reg_wr(ATH_SLIC_TX_SLOTS1, ATH_SLIC_TX_SLOTS1_EN(0x30003));
		ath_reg_wr(ATH_SLIC_RX_SLOTS1, ATH_SLIC_RX_SLOTS1_EN(0x30003));

		/* Set/Clear LONG FS according to user input */
		if (!ath_slic_long_fs) {
			ath_reg_rmw_clear(ATH_SLIC_TIMING_CTRL, ATH_SLIC_TIMING_CTRL_LONG_FS);
		} else {
			ath_reg_rmw_set(ATH_SLIC_TIMING_CTRL, ATH_SLIC_TIMING_CTRL_LONG_FS);
		}

/* Added for Sweep sample position. Experimental only. Yet to test */
#if 0
		/* Set/Clear Sweep Sample position */
		ath_reg_rmw_clear(ATH_SLIC_TIMING_CTRL, ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS(3));
		ath_reg_rmw_clear(ATH_SLIC_TIMING_CTRL, ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(3));
		ath_reg_rmw_set(ATH_SLIC_TIMING_CTRL, ((ATH_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS(ath_slic_rx_sweep_pos)) |
                                                (ATH_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(ath_slic_tx_sweep_pos))));
#endif
		/*
		 * FIFO RESET before enabling SLIC
		 */
		ath_slic_request_dma_channel();

		/*
		 * To Be Done
		 * Read, Modify write for SLIC enable
		 */
		if (ath_slic_slave) {
			//ath_reg_wr(ATH_SLIC_CTRL, 0x0a);
			ath_reg_wr(ATH_SLIC_CTRL, (ATH_SLIC_CTRL_EN  | ATH_SLIC_CTRL_SLAVE | 
						   ATH_SLIC_CTRL_CLK_EN)); 
		} else {
			//ath_reg_wr(ATH_SLIC_CTRL, 0x1e);
			ath_reg_wr(ATH_SLIC_CTRL, (ATH_SLIC_CTRL_EN | ATH_SLIC_CTRL_MASTER |
						   ATH_SLIC_CTRL_CLK_EN | SLIC_CTRL_PLL_SOURCE_SEL_SET(1)));
			/* Write MCLK SEL . PLL source could be audio PLL, ref clk 100, ref clk 125
			 * or from the Audio PLL */
			if (ath_slic_mclk_sel == 0) {
			/* Set Audio PLL registers */
			/* Power up PLL */
			ath_reg_rmw_set(AUDIO_PLL_CONFIG_ADDRESS,  AUDIO_PLL_CONFIG_PLLPWD_SET(1));
			ath_reg_rmw_clear(AUDIO_PLL_CONFIG_ADDRESS,  AUDIO_PLL_CONFIG_PLLPWD_SET(1));
			}

			ath_reg_rmw_clear(ATH_SLIC_CTRL, SLIC_CTRL_PLL_SOURCE_SEL_SET(3));
			ath_reg_rmw_set(ATH_SLIC_CTRL,  SLIC_CTRL_PLL_SOURCE_SEL_SET(ath_slic_mclk_sel));
		}
#endif
	} else {

//		if (ath_slic_slave) {
//			rddata = GPIO_IN_ENABLE4_SLIC_PCM_FS_IN_SET(14);
//		}
//		ath_reg_rmw_set(ATH_GPIO_OE, 1<<11);		
		
		rddata = ath_reg_rd(ATH_GPIO_IN_ENABLE4);
		rddata |= GPIO_IN_ENABLE4_SLIC_DATA_IN_SET(11);		
		ath_reg_wr(ATH_GPIO_IN_ENABLE4, rddata);
		
		rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION3);
#ifdef B6_V3 //B6_V3
		rddata |= (ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_12(0x4) |
				ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_14(0x6) |
				ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_15(0x5));
#else
		rddata |= (ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_12(0x4) |
				ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_13(0x6) |
				ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_14(0x5));
#endif
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);

//		ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, (ATH_GPIO_FUNCTION_ETH_SWITCH_LED3_EN |
//					ATH_GPIO_FUNCTION_ETH_SWITCH_LED1_EN));

		/* Disable JTAG. This is done since the SLIC pins are brought to 
		 * the EJTAG pins in WASP */
//		ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, ATH_GPIO_FUNCTION_JTAG_DISABLE);

		if (ath_slic_slave) {
			// Enable I2S EXT MCLK as input clk for SLIC on GPIO 13
			ath_reg_rmw_clear(ATH_GPIO_IN_ENABLE1, GPIO_IN_ENABLE1_I2SEXT_MCLK_SET(0xff));
			ath_reg_rmw_set(ATH_GPIO_IN_ENABLE1, GPIO_IN_ENABLE1_I2SEXT_MCLK_SET(1));

			// Enable SLIC PCM FS IN as input FS for slic on GPIO 12
			ath_reg_rmw_clear(ATH_GPIO_IN_ENABLE4, GPIO_IN_ENABLE4_SLIC_PCM_FS_IN_SET(0xff));
			ath_reg_rmw_set(ATH_GPIO_IN_ENABLE4, GPIO_IN_ENABLE4_SLIC_PCM_FS_IN_SET(2));

			rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION0);
			rddata = rddata & 0x00ffffff;
			rddata = rddata | ATH_GPIO_OUT_FUNCTION0_ENABLE_GPIO_3(7);
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION0, rddata);
		} else {
//			rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION0);
//			rddata = rddata & 0xff0000ff;
//			rddata = rddata | ATH_GPIO_OUT_FUNCTION0_ENABLE_GPIO_2(5) |
//				ATH_GPIO_OUT_FUNCTION0_ENABLE_GPIO_1(6);
//			ath_reg_wr(ATH_GPIO_OUT_FUNCTION0, rddata);
		}

		if (ath_slic_slave) {
			rddata = ATH_GPIO_OE_EN(0x30307);
		} else {
//			rddata = ATH_GPIO_OE_EN(0x30309);
			rddata = ath_reg_rd(ATH_GPIO_OE);
			rddata = rddata | ATH_GPIO_OE_EN(0x800);//GPIO11=SLIC_DATA_IN
#ifdef B6_V3 //B6_V3
			rddata = rddata & 0xffff2fff;//GPIO12=SLIC_DATA_OUT GPIO14=SLIC_PCM_CLK GPIO15=SLIC_PCM_FS  
#else
			rddata = rddata & 0xffff8fff;//GPIO12=SLIC_DATA_OUT GPIO13=SLIC_PCM_CLK GPIO14=SLIC_PCM_FS
#endif		
		}
		ath_reg_wr(ATH_GPIO_OE, rddata);

//		ath_reg_rmw_set(ATH_GPIO_OE, ATH_GPIO_OE_EN(1<<19));
//		ath_reg_rmw_clear(ATH_GPIO_OE, ATH_GPIO_OE_EN(1<<19));
//		ath_reg_rmw_set(ATH_GPIO_OE, ATH_GPIO_OE_EN(1<<19));

		ath_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_SLIC_RESET_SET(1));
	}

}

void ath_slic_request_dma_channel()
{
	int thresh = 2;
//	if (is_qca955x()) {
	if (0) {
		ath_reg_wr(ATH_MBOX_SLIC_FIFO_RESET, 0xFF);
		ath_reg_wr(ATH_MBOX_SLIC_INT_ENABLE, 0x440);
		thresh = 4;
	}
#if 1
	ath_reg_wr(ATH_MBOX_SLIC_DMA_POLICY, 
			(ATH_MBOX_DMA_POLICY_RX_QUANTUM |
			 ATH_MBOX_DMA_POLICY_TX_QUANTUM |
			 ATH_MBOX_DMA_POLICY_TX_FIFO_THRESH(thresh)));
#else 
	ath_reg_wr(ATH_MBOX_SLIC_DMA_POLICY, 
			(MBOX_DMA_POLICY_RXD_16BIT_SWAP_SET(1)|
			 MBOX_DMA_POLICY_TXD_16BIT_SWAP_SET(1)|
			 ATH_MBOX_DMA_POLICY_TX_FIFO_THRESH(thresh)));
#endif

	/* Set SRAM access bit */
#ifdef USE_SRAM
	ath_reg_rmw_set(ATH_MBOX_SLIC_DMA_POLICY, ATH_MBOX_DMA_POLICY_SRAM_AC);
#endif

}

void ath_slic_dma_start(unsigned long desc_buf_p, int mode)
{
//	if (is_ar934x() && !mode) {
		if(!mode){
		ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_SLIC_RESET_SET(1));
		ath_slic_request_dma_channel();
#if 1
		ath_reg_wr(ATH_MBOX_SLIC_FIFO_RESET, (ATH_MBOX_SLIC_FIFO_RESET_RX_INIT |
					ATH_MBOX_SLIC_FIFO_RESET_TX_INIT));
#else
		if (mode) {
			ath_reg_wr(ATH_MBOX_SLIC_FIFO_RESET, ATH_MBOX_SLIC_FIFO_RESET_TX_INIT);
		} else {
			ath_reg_wr(ATH_MBOX_SLIC_FIFO_RESET, ATH_MBOX_SLIC_FIFO_RESET_RX_INIT);
		}
#endif
//		ath_slic_enable();
	}

	/* Set up descriptor start address and Start DMA */
	if (mode) {
		printk("Tx Desc Base 0x%08lx, NUM_DESC %u, SLIC_BUF_SIZE %u ...\n", desc_buf_p, NUM_DESC, ath_slic_buf_size);
		ath_reg_wr(ATH_MBOX_DMA_TX_DESCRIPTOR_BASE1, desc_buf_p);
		ath_reg_wr(ATH_MBOX_DMA_TX_CONTROL1, ATH_MBOX_DMA_START);

		ath_reg_rmw_set(ATH_SLIC_CTRL, ATH_SLIC_CTRL_EN);
	} else {
		printk("Rx Desc Base 0x%08lx, NUM_DESC %u, SLIC_BUF_SIZE %u ...\n", desc_buf_p, NUM_DESC, ath_slic_buf_size);
		ath_reg_wr(ATH_MBOX_DMA_RX_DESCRIPTOR_BASE1, desc_buf_p);
		ath_reg_wr(ATH_MBOX_DMA_RX_CONTROL1, ATH_MBOX_DMA_START);
		if (is_ar934x()) {
			while (!(ath_reg_rd(ATH_MBOX_SLIC_FIFO_STATUS) & ATH_MBOX_SLIC_FIFO_STATUS_FULL))
			{
				printk("Wait FIFO full\n");
				mdelay(20);
			}
			ath_reg_rmw_set(ATH_SLIC_CTRL, ATH_SLIC_CTRL_EN);
		}
		//使能slic时钟(在打开写DMA才启动，所以应用层必须先打开读，再打开写)
		ath_slic_enable();
	}
	if (is_ar934x()) {
		udelay(20);
	}
}

void ath_slic_dma_pause(int mode)
{
	/*
	 * Pause
	 */
	if (mode) {
		ath_reg_wr(ATH_MBOX_DMA_TX_CONTROL1, ATH_MBOX_DMA_PAUSE);
	} else {
		ath_reg_wr(ATH_MBOX_DMA_RX_CONTROL1, ATH_MBOX_DMA_PAUSE);
	}
}

void ath_slic_dma_resume(int mode)
{
	/*
	 * Resume
	 */
	if (mode) {
		ath_reg_wr(ATH_MBOX_DMA_TX_CONTROL1, ATH_MBOX_DMA_RESUME);
	} else {
		ath_reg_wr(ATH_MBOX_DMA_RX_CONTROL1, ATH_MBOX_DMA_RESUME);
	}
}


loff_t ath_slic_llseek(struct file *filp, loff_t off, int whence)
{
	printk(KERN_CRIT "llseek\n");
	return off;
}

struct file_operations ath_slic_fops = {
	.owner = THIS_MODULE,
	.llseek = ath_slic_llseek,
	.read = ath_slic_read,
	.write = ath_slic_write,
	.ioctl = ath_slic_ioctl,
	.open = ath_slic_open,
	.release = ath_slic_close,
};
void ath_slic_cleanup_module(void)
{
	unsigned int j = 0;
	ath_slic_softc_t *sc = &sc_buf_var;

	slic_dma_buf_t *dmabuf_r, *dmabuf_p;


	printk(KERN_CRIT "unregister\n");
	
	ath_reg_rmw_clear(ATH_MBOX_SLIC_INT_ENABLE, 
				( ATH_MBOX_SLIC_TX_DMA_COMPLETE ));
	ath_reg_rmw_clear(ATH_MBOX_SLIC_INT_ENABLE, 
				( ATH_MBOX_SLIC_RX_DMA_COMPLETE ));
	dmabuf_r = &sc->sc_rbuf;
	dmabuf_p = &sc->sc_pbuf;


/* Free up allocated memories only if USE_SRAM is not 
 * defined */

#ifndef USE_SRAM
	for (j = 0; j < NUM_DESC; j++) {
		dma_unmap_single(NULL, dmabuf_r->db_buf[j].bf_paddr,
				ath_slic_buf_size, DMA_BIDIRECTIONAL);
		dma_unmap_single(NULL, dmabuf_p->db_buf[j].bf_paddr,
				ath_slic_buf_size, DMA_BIDIRECTIONAL);
	}

	kfree(sc->sc_rmall_buf);
	kfree(sc->sc_pmall_buf);
	dma_free_coherent(NULL,
			NUM_DESC * sizeof(ath_mbox_dma_desc),
			dmabuf_p->db_desc, dmabuf_p->db_desc_p);

	dma_free_coherent(NULL,
			NUM_DESC * sizeof(ath_mbox_dma_desc),
			dmabuf_r->db_desc, dmabuf_r->db_desc_p);
	
#endif

	sc->ropened = 0;
	sc->rpause = 0;
	sc->popened = 0;
	sc->ppause = 0;

	free_irq(sc->sc_irq, NULL);
	unregister_chrdev(ath_slic_major, "ath_slic");
}

/* Init Module called during insmod */
int ath_slic_init_module(void)
{
	ath_slic_softc_t *sc = &sc_buf_var;
	int result = -1;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (ath_slic_major) {
		result =
		    register_chrdev(ath_slic_major, "ath_slic",
				    &ath_slic_fops);
	}
	if (result < 0) {
		printk(KERN_WARNING "ath_slic: can't get major %d\n",
		       ath_slic_major);
		return result;
	}

	sc->sc_irq = ATH_MISC_IRQ_DMA;

	/* Establish ISR would take care of enabling the interrupt */
	result = request_irq(sc->sc_irq, ath_slic_intr, IRQF_DISABLED,
			     "ath_slic", NULL);
	if (result) {
		printk(KERN_INFO
		       "slic: can't get assigned irq %d returns %d\n",
		       sc->sc_irq, result);
	}

	/* GPIO init and reg init */
	ath_slic_link_on();

	ath_slic_set_freq();
	
	/* Allocate and initialize descriptors */
	if (ath_slic_init(SLIC_RX) == ENOMEM)
		return -ENOMEM;
	if (ath_slic_init(SLIC_TX) == ENOMEM)
		return -ENOMEM;
	
	//使能中断
//	ath_reg_rmw_set(ATH_MBOX_SLIC_INT_ENABLE, 
//				( ATH_MBOX_SLIC_TX_DMA_COMPLETE ));
//	ath_reg_rmw_set(ATH_MBOX_SLIC_INT_ENABLE, 
//				( ATH_MBOX_SLIC_RX_DMA_COMPLETE ));

	ath_slic_enable();//enable pclk out

	slic_dma_open=0;
//	si3217x_reset(1);
//	mdelay(1);
//	si3217x_reset(0);
	
//	si3217x_init();
	
//	SLIC_LOCK_INIT(&sc_buf_var);

	return 0;		/* succeed */
}

module_init(ath_slic_init_module);
module_exit(ath_slic_cleanup_module);

