#include <linux/dma-mapping.h>

/* Default Module param values */
#define DEF_MAX_SLOTS_8         32
#define DEF_SLOTS_EN_8          2
#define DEF_FS_LONG_1           1
#define DEF_TX_SWEEP_POS_1      1
#define DEF_RX_SWEEP_POS_0      0
#define DEF_MCLK_SEL_1          1

#define ATH_AUDIO_PLL_CFG_PWR_DWN   (1ul << 5)

#define ATH_AUDIO_DPLL2             0xb8116204
#define ATH_AUDIO_DPLL3             0xb8116208

#define ATH_AUDIO_DPLL2_RANGE           (1        << 31)
#define ATH_AUDIO_DPLL2_PATH_GAIN(x)    ((0xf&x)  << 26)
#define ATH_AUDIO_DPLL2_PROP_GAIN(x)    ((0x7f&x) << 19)
#define ATH_AUDIO_DPLL3_PHASE_SHIFT(x)  ((0x7f&x) << 23) 

#define ATH_AUD_DPLL2_RANGE_MSB			31
#define ATH_AUD_DPLL2_RANGE_LSB			31
#define ATH_AUD_DPLL2_RANGE_MASK		0x80000000
#define ATH_AUD_DPLL2_RANGE_GET(x)		(((x) & ATH_AUD_DPLL2_RANGE_MASK) >> ATH_AUD_DPLL2_RANGE_LSB)
#define ATH_AUD_DPLL2_RANGE_SET(x)		(((x) << ATH_AUD_DPLL2_RANGE_LSB) & ATH_AUD_DPLL2_RANGE_MASK)
#define ATH_AUD_DPLL2_RANGE_RESET		0x0 // 0
#define ATH_AUD_DPLL2_LOCAL_PLL_MSB		30
#define ATH_AUD_DPLL2_LOCAL_PLL_LSB		30
#define ATH_AUD_DPLL2_LOCAL_PLL_MASK		0x40000000
#define ATH_AUD_DPLL2_LOCAL_PLL_GET(x)		(((x) & ATH_AUD_DPLL2_LOCAL_PLL_MASK) >> ATH_AUD_DPLL2_LOCAL_PLL_LSB)
#define ATH_AUD_DPLL2_LOCAL_PLL_SET(x)		(((x) << ATH_AUD_DPLL2_LOCAL_PLL_LSB) & ATH_AUD_DPLL2_LOCAL_PLL_MASK)
#define ATH_AUD_DPLL2_LOCAL_PLL_RESET		0x0 // 0
#define ATH_AUD_DPLL2_KI_MSB			29
#define ATH_AUD_DPLL2_KI_LSB			26
#define ATH_AUD_DPLL2_KI_MASK			0x3c000000
#define ATH_AUD_DPLL2_KI_GET(x)			(((x) & ATH_AUD_DPLL2_KI_MASK) >> ATH_AUD_DPLL2_KI_LSB)
#define ATH_AUD_DPLL2_KI_SET(x)			(((x) << ATH_AUD_DPLL2_KI_LSB) & ATH_AUD_DPLL2_KI_MASK)
#define ATH_AUD_DPLL2_KI_RESET			0x6 // 6
#define ATH_AUD_DPLL2_KD_MSB			25
#define ATH_AUD_DPLL2_KD_LSB			19
#define ATH_AUD_DPLL2_KD_MASK			0x03f80000
#define ATH_AUD_DPLL2_KD_GET(x)			(((x) & ATH_AUD_DPLL2_KD_MASK) >> ATH_AUD_DPLL2_KD_LSB)
#define ATH_AUD_DPLL2_KD_SET(x)			(((x) << ATH_AUD_DPLL2_KD_LSB) & ATH_AUD_DPLL2_KD_MASK)
#define ATH_AUD_DPLL2_KD_RESET			0x7f // 127
#define ATH_AUD_DPLL2_EN_NEGTRIG_MSB		18
#define ATH_AUD_DPLL2_EN_NEGTRIG_LSB		18
#define ATH_AUD_DPLL2_EN_NEGTRIG_MASK		0x00040000
#define ATH_AUD_DPLL2_EN_NEGTRIG_GET(x)		(((x) & ATH_AUD_DPLL2_EN_NEGTRIG_MASK) >> ATH_AUD_DPLL2_EN_NEGTRIG_LSB)
#define ATH_AUD_DPLL2_EN_NEGTRIG_SET(x)		(((x) << ATH_AUD_DPLL2_EN_NEGTRIG_LSB) & ATH_AUD_DPLL2_EN_NEGTRIG_MASK)
#define ATH_AUD_DPLL2_EN_NEGTRIG_RESET		0x0 // 0
#define ATH_AUD_DPLL2_SEL_1SDM_MSB		17
#define ATH_AUD_DPLL2_SEL_1SDM_LSB		17
#define ATH_AUD_DPLL2_SEL_1SDM_MASK		0x00020000
#define ATH_AUD_DPLL2_SEL_1SDM_GET(x)		(((x) & ATH_AUD_DPLL2_SEL_1SDM_MASK) >> ATH_AUD_DPLL2_SEL_1SDM_LSB)
#define ATH_AUD_DPLL2_SEL_1SDM_SET(x)		(((x) << ATH_AUD_DPLL2_SEL_1SDM_LSB) & ATH_AUD_DPLL2_SEL_1SDM_MASK)
#define ATH_AUD_DPLL2_SEL_1SDM_RESET		0x0 // 0
#define ATH_AUD_DPLL2_PLL_PWD_MSB		16
#define ATH_AUD_DPLL2_PLL_PWD_LSB		16
#define ATH_AUD_DPLL2_PLL_PWD_MASK		0x00010000
#define ATH_AUD_DPLL2_PLL_PWD_GET(x)		(((x) & ATH_AUD_DPLL2_PLL_PWD_MASK) >> ATH_AUD_DPLL2_PLL_PWD_LSB)
#define ATH_AUD_DPLL2_PLL_PWD_SET(x)		(((x) << ATH_AUD_DPLL2_PLL_PWD_LSB) & ATH_AUD_DPLL2_PLL_PWD_MASK)
#define ATH_AUD_DPLL2_PLL_PWD_RESET		0x1 // 1
#define ATH_AUD_DPLL2_OUTDIV_MSB		15
#define ATH_AUD_DPLL2_OUTDIV_LSB		13
#define ATH_AUD_DPLL2_OUTDIV_MASK		0x0000e000
#define ATH_AUD_DPLL2_OUTDIV_GET(x)		(((x) & ATH_AUD_DPLL2_OUTDIV_MASK) >> ATH_AUD_DPLL2_OUTDIV_LSB)
#define ATH_AUD_DPLL2_OUTDIV_SET(x)		(((x) << ATH_AUD_DPLL2_OUTDIV_LSB) & ATH_AUD_DPLL2_OUTDIV_MASK)
#define ATH_AUD_DPLL2_OUTDIV_RESET		0x0 // 0
#define ATH_AUD_DPLL2_DELTA_MSB			12
#define ATH_AUD_DPLL2_DELTA_LSB			7
#define ATH_AUD_DPLL2_DELTA_MASK		0x00001f80
#define ATH_AUD_DPLL2_DELTA_GET(x)		(((x) & ATH_AUD_DPLL2_DELTA_MASK) >> ATH_AUD_DPLL2_DELTA_LSB)
#define ATH_AUD_DPLL2_DELTA_SET(x)		(((x) << ATH_AUD_DPLL2_DELTA_LSB) & ATH_AUD_DPLL2_DELTA_MASK)
#define ATH_AUD_DPLL2_DELTA_RESET		0x1e // 30
#define ATH_AUD_DPLL2_SPARE_MSB			6
#define ATH_AUD_DPLL2_SPARE_LSB			0
#define ATH_AUD_DPLL2_SPARE_MASK		0x0000007f
#define ATH_AUD_DPLL2_SPARE_GET(x)		(((x) & ATH_AUD_DPLL2_SPARE_MASK) >> ATH_AUD_DPLL2_SPARE_LSB)
#define ATH_AUD_DPLL2_SPARE_SET(x)		(((x) << ATH_AUD_DPLL2_SPARE_LSB) & ATH_AUD_DPLL2_SPARE_MASK)
#define ATH_AUD_DPLL2_SPARE_RESET		0x0 // 0
#define ATH_AUD_DPLL2_ADDRESS			0x18116204

#define ATH_AUD_DPLL3_MEAS_AT_TXON_MSB		31
#define ATH_AUD_DPLL3_MEAS_AT_TXON_LSB		31
#define ATH_AUD_DPLL3_MEAS_AT_TXON_MASK		0x80000000
#define ATH_AUD_DPLL3_MEAS_AT_TXON_GET(x)	(((x) & ATH_AUD_DPLL3_MEAS_AT_TXON_MASK) >> ATH_AUD_DPLL3_MEAS_AT_TXON_LSB)
#define ATH_AUD_DPLL3_MEAS_AT_TXON_SET(x)	(((x) << ATH_AUD_DPLL3_MEAS_AT_TXON_LSB) & ATH_AUD_DPLL3_MEAS_AT_TXON_MASK)
#define ATH_AUD_DPLL3_MEAS_AT_TXON_RESET	0x0 // 0
#define ATH_AUD_DPLL3_DO_MEAS_MSB		30
#define ATH_AUD_DPLL3_DO_MEAS_LSB		30
#define ATH_AUD_DPLL3_DO_MEAS_MASK		0x40000000
#define ATH_AUD_DPLL3_DO_MEAS_GET(x)		(((x) & ATH_AUD_DPLL3_DO_MEAS_MASK) >> ATH_AUD_DPLL3_DO_MEAS_LSB)
#define ATH_AUD_DPLL3_DO_MEAS_SET(x)		(((x) << ATH_AUD_DPLL3_DO_MEAS_LSB) & ATH_AUD_DPLL3_DO_MEAS_MASK)
#define ATH_AUD_DPLL3_DO_MEAS_RESET		0x0 // 0
#define ATH_AUD_DPLL3_PHASE_SHIFT_MSB		29
#define ATH_AUD_DPLL3_PHASE_SHIFT_LSB		23
#define ATH_AUD_DPLL3_PHASE_SHIFT_MASK		0x3f800000
#define ATH_AUD_DPLL3_PHASE_SHIFT_GET(x)	(((x) & ATH_AUD_DPLL3_PHASE_SHIFT_MASK) >> ATH_AUD_DPLL3_PHASE_SHIFT_LSB)
#define ATH_AUD_DPLL3_PHASE_SHIFT_SET(x)	(((x) << ATH_AUD_DPLL3_PHASE_SHIFT_LSB) & ATH_AUD_DPLL3_PHASE_SHIFT_MASK)
#define ATH_AUD_DPLL3_PHASE_SHIFT_RESET		0x0 // 0
#define ATH_AUD_DPLL3_SQSUM_DVC_MSB		22
#define ATH_AUD_DPLL3_SQSUM_DVC_LSB		3
#define ATH_AUD_DPLL3_SQSUM_DVC_MASK		0x007ffff8
#define ATH_AUD_DPLL3_SQSUM_DVC_GET(x)		(((x) & ATH_AUD_DPLL3_SQSUM_DVC_MASK) >> ATH_AUD_DPLL3_SQSUM_DVC_LSB)
#define ATH_AUD_DPLL3_SQSUM_DVC_SET(x)		(((x) << ATH_AUD_DPLL3_SQSUM_DVC_LSB) & ATH_AUD_DPLL3_SQSUM_DVC_MASK)
#define ATH_AUD_DPLL3_SQSUM_DVC_RESET		0x0 // 0
#define ATH_AUD_DPLL3_SPARE_MSB			2
#define ATH_AUD_DPLL3_SPARE_LSB			0
#define ATH_AUD_DPLL3_SPARE_MASK		0x00000007
#define ATH_AUD_DPLL3_SPARE_GET(x)		(((x) & ATH_AUD_DPLL3_SPARE_MASK) >> ATH_AUD_DPLL3_SPARE_LSB)
#define ATH_AUD_DPLL3_SPARE_SET(x)		(((x) << ATH_AUD_DPLL3_SPARE_LSB) & ATH_AUD_DPLL3_SPARE_MASK)
#define ATH_AUD_DPLL3_SPARE_RESET		0x0 // 0
#define ATH_AUD_DPLL3_ADDRESS			0x18116208

#define ATH_AUD_DPLL4_MEAN_DVC_MSB		31
#define ATH_AUD_DPLL4_MEAN_DVC_LSB		21
#define ATH_AUD_DPLL4_MEAN_DVC_MASK		0xffe00000
#define ATH_AUD_DPLL4_MEAN_DVC_GET(x)		(((x) & ATH_AUD_DPLL4_MEAN_DVC_MASK) >> ATH_AUD_DPLL4_MEAN_DVC_LSB)
#define ATH_AUD_DPLL4_MEAN_DVC_SET(x)		(((x) << ATH_AUD_DPLL4_MEAN_DVC_LSB) & ATH_AUD_DPLL4_MEAN_DVC_MASK)
#define ATH_AUD_DPLL4_MEAN_DVC_RESET		0x0 // 0
#define ATH_AUD_DPLL4_VC_MEAS0_MSB		20
#define ATH_AUD_DPLL4_VC_MEAS0_LSB		4
#define ATH_AUD_DPLL4_VC_MEAS0_MASK		0x001ffff0
#define ATH_AUD_DPLL4_VC_MEAS0_GET(x)		(((x) & ATH_AUD_DPLL4_VC_MEAS0_MASK) >> ATH_AUD_DPLL4_VC_MEAS0_LSB)
#define ATH_AUD_DPLL4_VC_MEAS0_SET(x)		(((x) << ATH_AUD_DPLL4_VC_MEAS0_LSB) & ATH_AUD_DPLL4_VC_MEAS0_MASK)
#define ATH_AUD_DPLL4_VC_MEAS0_RESET		0x0 // 0
#define ATH_AUD_DPLL4_MEAS_DONE_MSB		3
#define ATH_AUD_DPLL4_MEAS_DONE_LSB		3
#define ATH_AUD_DPLL4_MEAS_DONE_MASK		0x00000008
#define ATH_AUD_DPLL4_MEAS_DONE_GET(x)		(((x) & ATH_AUD_DPLL4_MEAS_DONE_MASK) >> ATH_AUD_DPLL4_MEAS_DONE_LSB)
#define ATH_AUD_DPLL4_MEAS_DONE_SET(x)		(((x) << ATH_AUD_DPLL4_MEAS_DONE_LSB) & ATH_AUD_DPLL4_MEAS_DONE_MASK)
#define ATH_AUD_DPLL4_MEAS_DONE_RESET		0x0 // 0
#define ATH_AUD_DPLL4_SPARE_MSB			2
#define ATH_AUD_DPLL4_SPARE_LSB			0
#define ATH_AUD_DPLL4_SPARE_MASK		0x00000007
#define ATH_AUD_DPLL4_SPARE_GET(x)		(((x) & ATH_AUD_DPLL4_SPARE_MASK) >> ATH_AUD_DPLL4_SPARE_LSB)
#define ATH_AUD_DPLL4_SPARE_SET(x)		(((x) << ATH_AUD_DPLL4_SPARE_LSB) & ATH_AUD_DPLL4_SPARE_MASK)
#define ATH_AUD_DPLL4_SPARE_RESET		0x0 // 0
#define ATH_AUD_DPLL4_ADDRESS			0x1811620c

#define ATH_AUD_DPLL3_KD_25			0x3d
#define ATH_AUD_DPLL3_KD_40			0x32

#define ATH_AUD_DPLL3_KI_25			0x4
#define ATH_AUD_DPLL3_KI_40			0x4



/* Maximum bits in a register */
#define MAX_REG_BITS            32
/* Enable all slots */
#define SLIC_SLOTS_EN_ALL       0xffffffff

/* Mode */
#define SLIC_RX                 1     /* MBOX Tx */   
#define SLIC_TX                 2     /* MBOX Rx */
/* For SRAM access. Use only for Scorpion. 
 * Strictly do not use for WASP or
 * older platforms */
#define USE_SRAM
/* Comment it out for SRAM access */
#undef USE_SRAM

#ifndef USE_SRAM
#define NUM_DESC              32
#define SLIC_BUF_SIZE         1024
#else
/* For SRAM */
#define NUM_DESC                4
#define SLIC_BUF_SIZE           128
#endif

#define SLIC_MIMR_TIMER 	_IOW('N', 0x20, int)
#define SLIC_DDR        	_IOW('N', 0x22, int)
#define SLIC_BITSWAP            _IOW('N', 0x23, int)
#define SLIC_BYTESWAP           _IOW('N', 0x24, int)
#define SI3000_REG           _IOW('N', 0x25, int)

//si3000 ioctl
struct si3000_ioctl_regs{
	int reg;
	char val;
};
/* driver i/o control command */
#define PCM_IOC_MAGIC	'p'
//#define _IO(type,nr)
#define PCM_SET_RECORD			_IO(PCM_IOC_MAGIC, 0)
#define PCM_SET_UNRECORD		_IO(PCM_IOC_MAGIC, 1)	
#define PCM_SET_PLAYBACK		_IO(PCM_IOC_MAGIC, 2)
#define PCM_SET_UNPLAYBACK		_IO(PCM_IOC_MAGIC, 3)
#define PCM_EXT_LOOPBACK_ON		_IO(PCM_IOC_MAGIC, 4)
#define PCM_EXT_LOOPBACK_OFF		_IO(PCM_IOC_MAGIC, 5)
#define	PCM_INT_LOOPBACK_ON		_IO(PCM_IOC_MAGIC, 6)
#define PCM_INT_LOOPBACK_OFF		_IO(PCM_IOC_MAGIC, 7)
#define	PCM_SI3000_REG			_IOW(PCM_IOC_MAGIC, 8, struct si3000_ioctl_regs)

/* control amplifier on/off */
#define	PCM_AMPLIFIER_EN			_IO(PCM_IOC_MAGIC, 9)
#define PCM_AMPLIFIER_DIS			_IO(PCM_IOC_MAGIC, 10)
/* control switch to pad/base */
#define	PCM_SWITCH_TO_PAD		_IO(PCM_IOC_MAGIC, 11)
#define PCM_SWITCH_TO_BASE		_IO(PCM_IOC_MAGIC, 12)
/* check pad insert status */
#define CHECK_PAD_STATUS		_IO(PCM_IOC_MAGIC, 13)
#define GET_SI3000_APP_PID_NUM		_IO(PCM_IOC_MAGIC, 14)

//end si3000 ioctl


#define SLIC_LOCK_INIT(_sc)	spin_lock_init(&(_sc)->slic_lock)
#define SLIC_LOCK_DESTROY(_sc)
#define SLIC_LOCK(_sc)		spin_lock_irqsave(&(_sc)->slic_lock, (_sc)->slic_lockflags)
#define SLIC_UNLOCK(_sc)	spin_unlock_irqrestore(&(_sc)->slic_lock, (_sc)->slic_lockflags)

typedef struct {
	unsigned int OWN        :  1,    /* bit 00 */
		     EOM        :  1,    /* bit 01 */
		     rsvd1      :  6,    /* bit 07-02 */
		     size       : 12,    /* bit 19-08 */
		     length     : 12,    /* bit 31-20 */
		     rsvd2      :  4,    /* bit 00 */
		     BufPtr     : 28,    /* bit 00 */
		     rsvd3      :  4,    /* bit 00 */
		     NextPtr    : 28,   /* bit 00 */
//		     Caligned   : 32;
		     Readable   : 1;
} ath_mbox_dma_desc;

typedef struct slic_buf {
	uint8_t *bf_vaddr;
	unsigned long bf_paddr;
} slic_buf_t;

typedef struct slic_dma_buf {
//	ath_mbox_dma_desc *lastbuf;
	ath_mbox_dma_desc *db_desc;
	dma_addr_t db_desc_p;
	slic_buf_t db_buf[NUM_DESC];
	int tail;
	int dma_tail;
} slic_dma_buf_t;

typedef struct ath_slic_softc {
	int ropened;
	int popened;
	slic_dma_buf_t sc_pbuf;
	slic_dma_buf_t sc_rbuf;
	char *sc_pmall_buf;
	char *sc_rmall_buf;
	int sc_irq;
	int ft_value;
	int ppause;
	int rpause;
	spinlock_t slic_lock;   /* lock */
	unsigned long slic_lockflags;
} ath_slic_softc_t;

ath_slic_softc_t sc_buf_var;
ath_slic_softc_t si_buf_var;


/* Function declarations */

/* Description : GPIO initializations and SLIC register
		 configurations 
 * Parameters  : None
 * Returns     : void */
void ath_slic_link_on(void);

/* Descritpion : To configure MBOX DMA Policy 
 * Paramaters  : None
 * Returns     : void */
void ath_slic_request_dma_channel(void);

/* Description : Program Tx and Rx descriptor base
		 addresses and start DMA 
 * Parameters  : desc_buf_p - Pointer to MBOX descriptor
                 mode       - Tx/Rx
 * Returns     : void */
void ath_slic_dma_start(unsigned long, int);

/* Description : Pause MBOX Tx/Rx DMA.
 * Parameters  : Mode - Tx/Rx
 * Returns     : void */
void ath_slic_dma_pause(int);

/* Description : Resume MBOX Tx/Rx DMA
 * Parameters  : Mode - Tx/Rx
 * Returns     : void */
void ath_slic_dma_resume(int);

/* Description : Unused function. Maintained for sanity
 * Parameters  : frac - frac value of PLL Modulation
                 pll - Audio PLL config 
 * Returns    : void */
void ar7242_slic_clk(unsigned long, unsigned long);

/* Description :  ISR to handle and clear MBOX interrupts
   Parameters  :  irq      - IRQ Number
                  dev_id   - Pointer to device data structure
                  regs     - Snapshot of processors context
   Returns     :  IRQ_HANDLED */
irqreturn_t ath_slic_intr(int irq, void *dev_id);

