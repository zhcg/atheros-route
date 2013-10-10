#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/version.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

#include "i2sio.h"
#include "ar7240.h"
#include "ar7240_i2s.h"		/* local definitions */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31))
#       define ar7240_dma_cache_sync(b, c)              \
        do {                                            \
                dma_cache_sync(NULL, (void *)b,         \
                                c, DMA_FROM_DEVICE);    \
        } while (0)
#       define ar7240_cache_inv(d, s)                   \
        do {                                            \
                dma_sync_single(NULL, (dma_addr_t)d, s, \
                                DMA_TO_DEVICE);         \
        } while (0)
#       define AR7240_IRQF_DISABLED     IRQF_DISABLED
#else
#       define ar7240_dma_cache_sync(b, c)              \
        do {                                            \
                dma_cache_wback((unsigned long)b, c);   \
        } while (0)
#       define ar7240_cache_inv(d, s)                   \
        do {                                            \
                dma_cache_inv((unsigned long)d, s);     \
        } while (0)
#       define AR7240_IRQF_DISABLED     SA_INTERRUPT
#endif

#define AOW_AUDIO_SYNC  1
#define AOW_AUDIO_SYNC_ENHANCEMENT 1
#define I2S_AOW_ENAB    1
#define USE_MEMCPY 1
#define MAX_I2S_WRITE_RETRIES 10
/* Keep the below in sync with WLAN driver code
   parameters */
#define AOW_PER_DESC_INTERVAL 125      /* In usec */
#define DESC_FREE_WAIT_BUFFER 200      /* In usec */

int ar7240_i2s_major = 253;
int ar7240_i2s_minor = 0;

static int i2s_start;

module_param(ar7240_i2s_major, int, S_IRUGO);
module_param(ar7240_i2s_minor, int, S_IRUGO);

MODULE_AUTHOR("Jacob@Atheros");
MODULE_LICENSE("Dual BSD/GPL");

void ar7240_i2sound_i2slink_on(int master);
void ar7240_i2sound_request_dma_channel(int);
void ar7240_i2sound_dma_desc(unsigned long, int);
void ar7240_i2sound_dma_start(int);
void ar7240_i2sound_dma_pause(int);
void ar7240_i2sound_dma_resume(int);
void ar7240_i2s_clk(unsigned long, unsigned long);
int  ar7242_i2s_open(void); 
void ar7242_i2s_close(void);
irqreturn_t ar7240_i2s_intr(int irq, void *dev_id, struct pt_regs *regs);


int ar7240_i2sound_dma_stopped(int);

/* 
 * XXX : This is the interface between i2s and wlan
 *       When adding info, here please make sure that
 *       it is reflected in the wlan side also
 */
typedef struct i2s_stats {
    unsigned int write_fail;
    unsigned int rx_underflow;
    unsigned int aow_sync_enabled;
    unsigned int sync_buf_full;
    unsigned int sync_buf_empty;
    unsigned int tasklet_count;
    unsigned int repeat_count;
} i2s_stats_t;    

i2s_stats_t stats;

/*
 * Functions and data structures relating to the Audio Sync feature
 *
 */

#if AOW_AUDIO_SYNC

/* tasklet related defines */

#define tq_struct tasklet_struct
#define I2S_INIT_TQUEUE(a, b, c)    tasklet_init((a), (b), (unsigned long)(c))
#define I2S_SCHEDULE_TQUEUE(a, b)   tasklet_schedule((a))
typedef unsigned long TQUEUE_ARG;

/* spin lock related */
#define I2S_SYNC_LOCK_INIT(_psinfo) spin_lock_init(&(_psinfo)->lock)
#define I2S_SYNC_LOCK_DESTROY(_psinfo)   
#define I2S_SYNC_IRQ_LOCK(_psinfo)      spin_lock_irqsave(&(_psinfo)->lock, (_psinfo)->lock_flags)
#define I2S_SYNC_IRQ_UNLOCK(_psinfo)    spin_unlock_irqrestore(&(_psinfo)->lock, (_psinfo)->lock_flags)
#define I2S_SYNC_LOCK(_psinfo)      spin_lock(&(_psinfo)->lock)
#define I2S_SYNC_UNLOCK(_psinfo)    spin_unlock(&(_psinfo)->lock)

#define I2S_OPENED(_sc)     (_sc->popened)

/* general defines */
#define TRUE    1
#define FALSE   !(TRUE)

/* i2s related defines */
#define MAX_AOW_DATA_SIZE   24
#define MAX_AOW_BUFFERS     1024


typedef enum {
    SYNC_IDLE,
    SYNC_PLAY,
}_sync_state;

/*
 * Single buffer to hold 4 to 8 ms worth of audio
 * data 
 */
typedef struct i2s_buffer {
    int length;
    char    data[MAX_AOW_DATA_SIZE];
}_i2s_buffer;


/*
 * Audio sync buffer, will hold multiple audio buffers
 */

typedef struct aow_sync_buffers {
    int read_index;     /* Read pointer */
    int write_index;    /* Write pointer */
    int count;          /* Total number packets in quque */
    int prev_index;     /* Last packet pointer */
    int audio_stopped;  /* Control flag */
    spinlock_t lock;            /* Lock */
	unsigned long lock_flags;   /* Lock flags */
    _sync_state state;          /* State */
    _i2s_buffer sb[MAX_AOW_BUFFERS];    /* Audio data buffer */
}_aow_sync_buffers;    


#define AOW_SET_AUDIO_STOPPED(p)    (p.audio_stopped = TRUE)
#define AOW_IS_AUDIO_STOPPED(p)     ((p.audio_stopped)?TRUE:FALSE)
#define AOW_RESET_AUDIO_STOPPED(p)  (p.audio_stopped = FALSE)

/* sync buffer */
struct aow_sync_buffers sinfo;

/* function prototypes */
static void i2s_rx_tasklet(TQUEUE_ARG data);
static void init_sync_buffers(struct aow_sync_buffers *p);
static int put_pkt(char *data, int len);
static int get_pkt(int *index);
ssize_t ar7240_i2s_write(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos, int resume);
int i2s_write_sbuff(char *data, int len);


/* 
 * The wlan will call this API, once it detects the stop
 * of audio data transfers, this function will do the necessary
 * cleanup and re-initialization as necessary
 *
 */
void i2s_audio_data_stop(void)
{
    I2S_SYNC_IRQ_LOCK(&sinfo);
    AOW_SET_AUDIO_STOPPED(sinfo);
    /* Reset the SYNC buffer params */
    sinfo.read_index = 0;
    sinfo.write_index = 0;
    sinfo.prev_index = 0;
    sinfo.state = SYNC_IDLE;
    I2S_SYNC_IRQ_UNLOCK(&sinfo);
    return;
}EXPORT_SYMBOL(i2s_audio_data_stop);


/*
 * The tasklet, on write done, we schedule the 
 * next packet in the queue to the i2s dma descriptor
 *
 */
static void i2s_rx_tasklet(TQUEUE_ARG data) 
{
    int index = 0;
    int result = 0;

    stats.tasklet_count++;

    if ((result = get_pkt(&index)) == TRUE) {
        ar7240_i2s_write(NULL, sinfo.sb[index].data, sinfo.sb[index].length, NULL, 1);
    }
#if AOW_AUDIO_SYNC_ENHANCEMENT    
    else if (!AOW_IS_AUDIO_STOPPED(sinfo)) {
        ar7240_i2s_write(NULL, sinfo.sb[sinfo.prev_index].data, sinfo.sb[sinfo.prev_index].length, NULL, 1);
        stats.repeat_count++;
    }        
#endif  /* AOW_AUDIO_SYNC_ENHANCEMENT */        

    return;
}

/* init the sync buffers */
static void init_sync_buffers(struct aow_sync_buffers *p)
{
    p->read_index = 0;
    p->write_index = 0;
    p->count = 0;
    p->prev_index = 0;
    p->state = SYNC_IDLE;
    I2S_SYNC_LOCK_INIT(p);

    return;
}    

/* API to insert packet, into sync buffer */
static int put_pkt(char *data, int len)
{

    if (sinfo.write_index == ((sinfo.read_index - 1 + MAX_AOW_BUFFERS) % MAX_AOW_BUFFERS)) {
        stats.sync_buf_full++;
        return -1;
    }

    memcpy(&sinfo.sb[sinfo.write_index].data, data, len);
    sinfo.sb[sinfo.write_index].length = len;
    sinfo.write_index = (sinfo.write_index + 1) % MAX_AOW_BUFFERS;
    sinfo.count++;
    AOW_RESET_AUDIO_STOPPED(sinfo);

    return 0;
}    


/*
 * function : ar7240_i2s_fill_dma
 * ------------------------------
 * The data will be buffered in the SYNC buffer, wlan will call this function
 * just before giving start to fill the dma, this will avoid spurious buffer
 * underflow conditions
 *
 */

int ar7240_i2s_fill_dma()
{
    I2S_SYNC_IRQ_LOCK(&sinfo);
    if ((sinfo.state == SYNC_IDLE)) {
        int index = 0;
        int pktcount = 0;
        /* Only populate NUM_DEC - 10, this is heuristic value */
        while ((get_pkt(&index) == TRUE) && ((pktcount++) <= (NUM_DESC - 10)))
            ar7240_i2s_write(NULL, sinfo.sb[index].data, sinfo.sb[index].length, NULL, 1);
        sinfo.state = SYNC_PLAY;
    }        
    I2S_SYNC_IRQ_UNLOCK(&sinfo);
    return 0;
}EXPORT_SYMBOL(ar7240_i2s_fill_dma);

/* API to get the packet index from the sync buffer */
static int get_pkt(int *index)
{
    if (sinfo.write_index == sinfo.read_index) {
        sinfo.state = SYNC_IDLE;
        sinfo.count = 0;
        stats.sync_buf_empty++;
        return FALSE;
    }

    *index = sinfo.read_index;
    sinfo.prev_index = sinfo.read_index;    /* Mark the current packet index */
    sinfo.read_index = (sinfo.read_index + 1) % MAX_AOW_BUFFERS;
    sinfo.count--;
    return TRUE;
}    

/* API : Wrapper to put packets into the sync buffers */
int i2s_write_sbuff(char *data, int len)
{
    return put_pkt(data, len);
}EXPORT_SYMBOL(i2s_write_sbuff);


#endif  /* AOW_AUDIO_SYNC */

int ar7240_i2sound_dma_stopped(int);


typedef struct i2s_buf {
	uint8_t *bf_vaddr;
	unsigned long bf_paddr;
} i2s_buf_t;


typedef struct i2s_dma_buf {
	ar7240_mbox_dma_desc *lastbuf;
	ar7240_mbox_dma_desc *db_desc;
	dma_addr_t db_desc_p;
	i2s_buf_t db_buf[NUM_DESC];
	int tail;
} i2s_dma_buf_t;

typedef struct ar7240_i2s_softc {
	int ropened;
	int popened;
	i2s_dma_buf_t sc_pbuf;
	i2s_dma_buf_t sc_rbuf;
	char *sc_pmall_buf;
	char *sc_rmall_buf;
	int sc_irq;
	int ft_value;
	int ppause;
	int rpause;
	spinlock_t i2s_lock;   /* lock */
#if AOW_AUDIO_SYNC    
    struct tq_struct i2s_tq;    /* tasklet */
#endif  /* AOW_AUDIO_SYNC */    
	unsigned long i2s_lockflags;
} ar7240_i2s_softc_t;


ar7240_i2s_softc_t sc_buf_var;
i2s_stats_t stats;

void i2s_get_stats(i2s_stats_t *p)
{
#if AOW_AUDIO_SYNC
    stats.aow_sync_enabled = TRUE;
#endif  /* AOW_AUDIO_SYNC */
    memcpy(p, &stats, sizeof(struct i2s_stats));
}EXPORT_SYMBOL(i2s_get_stats);    

void i2s_clear_stats()
{
   stats.write_fail = 0; 
   stats.rx_underflow = 0;
   stats.sync_buf_full = 0;
   stats.sync_buf_empty = 0;
   stats.tasklet_count = 0;
   stats.repeat_count = 0;
}EXPORT_SYMBOL(i2s_clear_stats);


int ar7242_i2s_desc_busy(struct file *filp)
{
    int mode;
    int own;
    int ret = 0;
    int j = 0;
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	ar7240_mbox_dma_desc *desc;
	i2s_dma_buf_t *dmabuf;

    if (!filp)
        mode = 0;
    else
        mode = filp->f_mode;

    if (mode & FMODE_READ) {
        dmabuf = &sc->sc_rbuf;
        own = sc->rpause;
    } else {
        dmabuf = &sc->sc_pbuf;
        own = sc->ppause;
    }        

    desc = dmabuf->db_desc;

    for (j = 0; j < NUM_DESC; j++) {
        if (desc[j].OWN) {
            desc[j].OWN = 0;
        }
        ar7240_i2sound_dma_resume(0);
    }        
    return ret;
}EXPORT_SYMBOL(ar7242_i2s_desc_busy);

int ar7240_i2s_init(struct file *filp)
{
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	i2s_buf_t *scbuf;
	uint8_t *bufp = NULL;
	int j, k, byte_cnt, tail = 0, mode = 1;
	ar7240_mbox_dma_desc *desc;
	unsigned long desc_p;

    if (!filp) {
        mode = FMODE_WRITE;
    } else {
        mode = filp->f_mode;
    }

	if (mode & FMODE_READ) {
			dmabuf = &sc->sc_rbuf;
			sc->ropened = 1;
			sc->rpause = 0;
		} else {
			dmabuf = &sc->sc_pbuf;
			sc->popened = 1;
			sc->ppause = 0;
		}

		dmabuf->db_desc = (ar7240_mbox_dma_desc *)
		    dma_alloc_coherent(NULL,
				       NUM_DESC *
				       sizeof(ar7240_mbox_dma_desc),
				       &dmabuf->db_desc_p, GFP_DMA);

		if (dmabuf->db_desc == NULL) {
			printk(KERN_CRIT "DMA desc alloc failed for %d\n",
		       mode);
			return ENOMEM;
		}

		for (j = 0; j < NUM_DESC; j++) {
			dmabuf->db_desc[j].OWN = 0;
#ifdef SPDIF
			for (k = 0; k < 6; k++) {
				dmabuf->db_desc[j].Va[k] = 0;
				dmabuf->db_desc[j].Ua[k] = 0;
				dmabuf->db_desc[j].Ca[k] = 0;
				dmabuf->db_desc[j].Vb[k] = 0;
				dmabuf->db_desc[j].Ub[k] = 0;
				dmabuf->db_desc[j].Cb[k] = 0;
			}
#if 0
            /* 16 Bit, 44.1 KHz */
			dmabuf->db_desc[j].Ca[0] = 0x00100000;
			dmabuf->db_desc[j].Ca[1] = 0x000000f2;
			dmabuf->db_desc[j].Cb[0] = 0x00200000;
			dmabuf->db_desc[j].Cb[1] = 0x000000f2;
            /* 16 Bit, 48 KHz */
			dmabuf->db_desc[j].Ca[0] = 0x02100000;
			dmabuf->db_desc[j].Ca[1] = 0x000000d2;
			dmabuf->db_desc[j].Cb[0] = 0x02200000;
			dmabuf->db_desc[j].Cb[1] = 0x000000d2;
#endif
            /* For Dynamic Conf */
            dmabuf->db_desc[j].Ca[0] |= SPDIF_CONFIG_CHANNEL(SPDIF_MODE_LEFT);
            dmabuf->db_desc[j].Cb[0] |= SPDIF_CONFIG_CHANNEL(SPDIF_MODE_RIGHT);
#ifdef SPDIFIOCTL
			dmabuf->db_desc[j].Ca[0] = 0x00100000;
			dmabuf->db_desc[j].Ca[1] = 0x02100000;
			dmabuf->db_desc[j].Cb[0] = 0x00200000;
			dmabuf->db_desc[j].Cb[1] = 0x02100000;
#endif
#endif
		}

		/* Allocate data buffers */
		scbuf = dmabuf->db_buf;

		if (!(bufp = kmalloc(NUM_DESC * I2S_BUF_SIZE, GFP_KERNEL))) {
			printk(KERN_CRIT
			       "Buffer allocation failed for \n");
			goto fail3;
		}

	if (mode & FMODE_READ)
			sc->sc_rmall_buf = bufp;
		else
			sc->sc_pmall_buf = bufp;

		for (j = 0; j < NUM_DESC; j++) {
			scbuf[j].bf_vaddr = &bufp[j * I2S_BUF_SIZE];
			scbuf[j].bf_paddr =
			    dma_map_single(NULL, scbuf[j].bf_vaddr,
					   I2S_BUF_SIZE,
					   DMA_BIDIRECTIONAL);

		}
		dmabuf->tail = 0;

		// Initialize desc
			desc = dmabuf->db_desc;
			desc_p = (unsigned long) dmabuf->db_desc_p;
			byte_cnt = NUM_DESC * I2S_BUF_SIZE;
			tail = dmabuf->tail;

			while (byte_cnt && (tail < NUM_DESC)) {
				desc[tail].rsvd1 = 0;
				desc[tail].size = I2S_BUF_SIZE;
				if (byte_cnt > I2S_BUF_SIZE) {
					desc[tail].length = I2S_BUF_SIZE;
					byte_cnt -= I2S_BUF_SIZE;
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
			      (sizeof(ar7240_mbox_dma_desc))));
		if (mode & FMODE_READ) {
					desc[tail].OWN = 1;
				} else {
					desc[tail].OWN = 0;
				}
				tail++;
			}
			tail--;
			desc[tail].NextPtr = desc_p;

		dmabuf->tail = 0;

#if AOW_AUDIO_SYNC   
    init_sync_buffers(&sinfo);
#endif  /* AOW_AUDIO_SYNC */

	return 0;

fail3:
	if (mode & FMODE_READ)
			dmabuf = &sc->sc_rbuf;
		else
			dmabuf = &sc->sc_pbuf;
		dma_free_coherent(NULL,
				  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
				  dmabuf->db_desc, dmabuf->db_desc_p);
	if (mode & FMODE_READ) {
			if (sc->sc_rmall_buf)
				kfree(sc->sc_rmall_buf);
		} else {
			if (sc->sc_pmall_buf)
				kfree(sc->sc_pmall_buf);
		}

	return -ENOMEM;

}

int ar7242_i2s_open()
{
    ar7240_i2s_softc_t *sc = &sc_buf_var;
    int opened = 0, mode = MASTER;

    i2s_start = 1;

#ifndef I2S_AOW_ENAB    
    ar7240_i2sound_i2slink_on(1);
#endif  /* I2S_AOW_ENAB */    

    if (sc->popened) {
        printk("%s, %d I2S speaker busy\n", __func__, __LINE__);
        return -EBUSY;
    }

    opened = (sc->ropened | sc->popened);

    /* Reset MBOX FIFO's */
    if (!opened) {
        ar7240_reg_wr(MBOX_FIFO_RESET, 0xff); // virian
        udelay(500);
    }

    /* Allocate and initialize descriptors */
    if (ar7240_i2s_init(NULL) == ENOMEM) {
        return -ENOMEM;
    }        

    if (!opened) {
        ar7240_i2sound_request_dma_channel(mode);
    }

    return (0);
}
EXPORT_SYMBOL(ar7242_i2s_open);

int ar7240_i2s_open(struct inode *inode, struct file *filp)
{

	ar7240_i2s_softc_t *sc = &sc_buf_var;
	int opened = 0, mode = MASTER;


	if ((filp->f_mode & FMODE_READ) && (sc->ropened)) {
        printk("%s, %d I2S mic busy\n", __func__, __LINE__);
		return -EBUSY;
	}
	if ((filp->f_mode & FMODE_WRITE) && (sc->popened)) {
        printk("%s, %d I2S speaker busy\n", __func__, __LINE__);
		return -EBUSY;
	}

	opened = (sc->ropened | sc->popened);

	/* Reset MBOX FIFO's */
	if (!opened) {
#if 0
		if (filp->f_mode & FMODE_READ) {
			ar7240_reg_wr(MBOX_FIFO_RESET, 0x01);
		} else {
			ar7240_reg_wr(MBOX_FIFO_RESET, 0x04);
		}
#else
		ar7240_reg_wr(MBOX_FIFO_RESET, 0xff); // virian
#endif
		udelay(500);
	}

	/* Allocate and initialize descriptors */
	if (ar7240_i2s_init(filp) == ENOMEM)
		return -ENOMEM;

	if (!opened) {
	    ar7240_i2sound_request_dma_channel(mode);
    }

	return (0);

}


ssize_t ar7240_i2s_read(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos)
{
#define prev_tail(t) ({ (t == 0) ? (NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	struct ar7240_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf = &sc->sc_rbuf;
	i2s_buf_t *scbuf;
	ar7240_mbox_dma_desc *desc;
	unsigned int byte_cnt, mode = 1, offset = 0, tail = dmabuf->tail;
	unsigned long desc_p;
	int need_start = 0;

	byte_cnt = count;

	if (sc->ropened < 2) {
		ar7240_reg_rmw_set(MBOX_INT_ENABLE, MBOX0_TX_DMA_COMPLETE);
		need_start = 1;
	}

	sc->ropened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ar7240_mbox_dma_desc);

	while (byte_cnt && !desc[tail].OWN) {
		if (byte_cnt >= I2S_BUF_SIZE) {
			desc[tail].length = I2S_BUF_SIZE;
			byte_cnt -= I2S_BUF_SIZE;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
		}
		ar7240_dma_cache_sync(scbuf[tail].bf_vaddr, desc[tail].length);
		retval =
		    copy_to_user(buf + offset, scbuf[tail].bf_vaddr, 
				   I2S_BUF_SIZE);
		if (retval)
			return retval;
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].OWN = 1;

		tail = next_tail(tail);
		offset += I2S_BUF_SIZE;
	}

	dmabuf->tail = tail;

	if (need_start) {
		ar7240_i2sound_dma_desc((unsigned long) desc_p, mode);
        if (filp) {
		    ar7240_i2sound_dma_start(mode);
        }
	} else if (!sc->rpause) {
		ar7240_i2sound_dma_resume(mode);
	}

	return offset;
}

void ar7242_i2s_fifo_reset()
{
    ar7240_reg_wr(MBOX_FIFO_RESET, 0xff);
    udelay(500);

}EXPORT_SYMBOL(ar7242_i2s_fifo_reset);


ssize_t ar7240_i2s_write(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos, int resume)
{
#define prev_tail(t) ({ (t == 0) ? (NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	int byte_cnt, offset, need_start = 0;
	int mode = 0;
	struct ar7240_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf = &sc->sc_pbuf;
	i2s_buf_t *scbuf;
	ar7240_mbox_dma_desc *desc;
	int tail = dmabuf->tail;
	unsigned long desc_p;

	byte_cnt = count;

	if (sc->popened < 2) {
        ar7240_reg_rmw_set(MBOX_INT_ENABLE, MBOX0_RX_DMA_COMPLETE | RX_UNDERFLOW);
		need_start = 1;
	}

	sc->popened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	offset = 0;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ar7240_mbox_dma_desc);

	while (byte_cnt && !desc[tail].OWN) {
        if (byte_cnt >= I2S_BUF_SIZE) {
			desc[tail].length = I2S_BUF_SIZE;
			byte_cnt -= I2S_BUF_SIZE;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
		}
#if USE_MEMCPY        
        memcpy(scbuf[tail].bf_vaddr, buf + offset, I2S_BUF_SIZE);
#else        
		retval =
		    copy_from_user(scbuf[tail].bf_vaddr, buf + offset,
				   I2S_BUF_SIZE);
		if (retval)
			return retval;
#endif            
		ar7240_cache_inv(scbuf[tail].bf_vaddr, desc[tail].length);

		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].OWN = 1;

		tail = next_tail(tail);
		offset += I2S_BUF_SIZE;
	}

	dmabuf->tail = tail;

	if (need_start) {
		ar7240_i2sound_dma_desc((unsigned long) desc_p, mode);
	} 
    
//    if (resume)
//        ar7240_i2sound_dma_resume(mode);

	return count - byte_cnt;
}


void ar7242_i2s_write(size_t count, const char * buf)
{
    int tmpcount, ret = 0;
    int cnt = 0;
    char *data;
#if 0
    static int myVectCnt = 0; 
    u_int16_t *locBuf = (u_int16_t *)buf;
    for( tmpcount = 0; tmpcount < (count>>1); tmpcount++) {
        *locBuf++ = myTestVect[myVectCnt++];        
        if( myVectCnt >= (myTestVectSz *2) ) myVectCnt = 0;
    }
#endif

#if AOW_AUDIO_SYNC
    I2S_SYNC_IRQ_LOCK(&sinfo);
    i2s_write_sbuff(buf, count);
    I2S_SYNC_IRQ_UNLOCK(&sinfo);
    return;
#endif  /* AOW_AUDIO_SYNC */

eagain:
    tmpcount = count;
    data = buf;
    ret = 0;

    do {
        ret = ar7240_i2s_write(NULL, data, tmpcount, NULL, 1);
        cnt++;
        if (ret == EAGAIN) {
            printk("%s:%d %d\n", __func__, __LINE__, ret);
            goto eagain;
        }

        tmpcount = tmpcount - ret;
        data += ret;
#ifdef  I2S_AOW_ENAB
        if (cnt > MAX_I2S_WRITE_RETRIES) {
            if (i2s_start) {
                //ar7240_i2sound_dma_start(0);
                i2s_start = 0;
            }
            stats.write_fail++;
            printk("... %s, %d: stats %d....\n", __func__, __LINE__, stats.write_fail);
            return;
        }            
#endif
    } while(tmpcount);
}
EXPORT_SYMBOL(ar7242_i2s_write);

ssize_t ar9100_i2s_write(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos)
{
    int tmpcount, ret = 0;
    int cnt = 0;
    char *data;
#if 0
    static int myVectCnt = 0;
    u_int16_t *locBuf = (u_int16_t *)buf;
    for( tmpcount = 0; tmpcount < (count>>1); tmpcount++) {
        *locBuf++ = myTestVect[myVectCnt++];
        if( myVectCnt >= (myTestVectSz *2) ) myVectCnt = 0;
    }
#endif
eagain:
    tmpcount = count;
    data = buf;
    ret = 0;

    do {
        ret = ar7240_i2s_write(NULL, data, tmpcount, NULL, 1);
        cnt++;
        if (ret == EAGAIN) {
            printk("%s:%d %d\n", __func__, __LINE__, ret);
            goto eagain;
        }

        tmpcount = tmpcount - ret;
        data += ret;
#if I2S_AOW_ENAB
        if (cnt > MAX_I2S_WRITE_RETRIES) {
            stats.write_fail++;
            return;
        }            
#endif
    } while(tmpcount);

    return 0;

}


int ar7240_i2s_close(struct inode *inode, struct file *filp)
{
	int j, own, mode;
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	ar7240_mbox_dma_desc *desc;
    int status = TRUE;
    int own_count = 0;

    if (!filp) {
        mode  = 0;
    } else {
        mode = filp->f_mode;
    }

	if (mode & FMODE_READ) {
		dmabuf = &sc->sc_rbuf;
		own = sc->rpause;
	} else {
		dmabuf = &sc->sc_pbuf;
		own = sc->ppause;
	}

	desc = dmabuf->db_desc;

	if (own) {
		for (j = 0; j < NUM_DESC; j++) {
			desc[j].OWN = 0;
		}
		ar7240_i2sound_dma_resume(mode);
    } else {
        for (j = 0; j < NUM_DESC; j++) {
            if (desc[j].OWN) {
                own_count++;
            }
        }
        
        /* 
         * The schedule_timeout_interruptible is commented
         * as this function is called from other process
         * context, i.e. that of wlan device driver context
         * schedule_timeout_interruptible(HZ);
         */

        if (own_count > 0) { 
            udelay((own_count * AOW_PER_DESC_INTERVAL) + DESC_FREE_WAIT_BUFFER);
            
            for (j = 0; j < NUM_DESC; j++) {
                /* break if the descriptor is still not free*/
                if (desc[j].OWN) {
                    status = FALSE;
                    printk("I2S : Fatal error\n");
                    break;
                }
            }
        }
    }

	for (j = 0; j < NUM_DESC; j++) {
		dma_unmap_single(NULL, dmabuf->db_buf[j].bf_paddr,
				 I2S_BUF_SIZE, DMA_BIDIRECTIONAL);
	}

	if (mode & FMODE_READ)
		kfree(sc->sc_rmall_buf);
	else
		kfree(sc->sc_pmall_buf);
	dma_free_coherent(NULL,
			  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
			  dmabuf->db_desc, dmabuf->db_desc_p);

	if (mode & FMODE_READ) {
		sc->ropened = 0;
		sc->rpause = 0;
	} else {
		sc->popened = 0;
		sc->ppause = 0;
	}

	return (status);
}

void ar7242_i2s_close()          
{                                
    ar7240_i2s_close(NULL, NULL);
}                                
EXPORT_SYMBOL(ar7242_i2s_close); 

int ar7240_i2s_release(struct inode *inode, struct file *filp)
{
	printk(KERN_CRIT "release\n");
	return 0;
}

int ar7240_i2s_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
#define AR7240_STEREO_CONFIG_DEFAULT (AR7240_STEREO_CONFIG_SPDIF_ENABLE | \
                AR7240_STEREO_CONFIG_ENABLE | \
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE | \
                AR7240_STEREO_CONFIG_MASTER | \
                AR7240_STEREO_CONFIG_PSEDGE(2))

	int data, mask = 0, cab = 0, cab1 = 0, j, st_cfg = 0;
	struct ar7240_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;

    if (filp->f_mode & FMODE_READ) {
        dmabuf = &sc->sc_rbuf;
    } else {
        dmabuf = &sc->sc_pbuf;
    }

	switch (cmd) {
    case I2S_PAUSE:
        data = arg;
	ar7240_i2sound_dma_pause(data);
	if (data) {
		sc->rpause = 1;
	} else {
		sc->ppause = 1;
	}
        return 0;
    case I2S_RESUME:
        data = arg;
	ar7240_i2sound_dma_resume(data);
	if (data) {
		sc->rpause = 0;
	} else {
		sc->ppause = 0;
	}
        return 0;
	case I2S_VOLUME:
		data = arg;
		if (data < 15) {
			if (data < 0) {
				mask = 0xf;
			} else {
				mask = (~data) & 0xf;
				mask = mask | 0x10;
			}
		} else {
			if (data <= 22) {
				if (data == 15) {
					data = 0;
				} else {
					mask = data - 15;
				}
			} else {
				mask = 7;
			}
		}
		data = mask | (mask << 8);
		ar7240_reg_wr(STEREO0_VOLUME, data);
		return 0;

	case I2S_FREQ:		/* Frequency settings */
		data = arg;
        switch (data) {
            case 44100:
                ar7240_i2s_clk(0x0a47f028, 0x2383);
                cab = SPDIF_CONFIG_SAMP_FREQ(SPDIF_SAMP_FREQ_44);
                cab1 = SPDIF_CONFIG_ORG_FREQ(SPDIF_ORG_FREQ_44);
                break;
            case 48000:
                ar7240_i2s_clk(0x03c9f02c, 0x2383);
                cab = SPDIF_CONFIG_SAMP_FREQ(SPDIF_SAMP_FREQ_48);
                cab1 = SPDIF_CONFIG_ORG_FREQ(SPDIF_ORG_FREQ_48);
                break;
            default:
                printk(KERN_CRIT "Freq %d not supported \n",
                   data);
                return -ENOTSUPP;
        }
        for (j = 0; j < NUM_DESC; j++) {
            dmabuf->db_desc[j].Ca[0] |= cab;
            dmabuf->db_desc[j].Cb[0] |= cab;
            dmabuf->db_desc[j].Ca[1] |= cab1;
            dmabuf->db_desc[j].Cb[1] |= cab1;
        }
		return 0;

	case I2S_FINE:
		data = arg;
		return 0;

	case I2S_DSIZE:
		data = arg;
		switch (data) {
		case 8:
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT | 
                 AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_8B));
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_8_16);
            break;
		case 16:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa21302);
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT | 
                AR7240_STEREO_CONFIG_PCM_SWAP |
                 AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_16B));
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_8_16);
            break;
		case 24:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa22b02);
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT | 
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_24B) |
                 AR7240_STEREO_CONFIG_I2S_32B_WORD);
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_24_32);
            break;
		case 32:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa22b02);
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT | 
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_32B) |
                 AR7240_STEREO_CONFIG_I2S_32B_WORD);
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_24_32);
            break;
		default:
			printk(KERN_CRIT "Data size %d not supported \n",
			       data);
			return -ENOTSUPP;
		}
        ar7240_reg_wr(AR7240_STEREO_CONFIG, (st_cfg | AR7240_STEREO_CONFIG_RESET));
        udelay(100);
        ar7240_reg_wr(AR7240_STEREO_CONFIG, st_cfg);
        for (j = 0; j < NUM_DESC; j++) {
            dmabuf->db_desc[j].Ca[1] |= cab1;
            dmabuf->db_desc[j].Cb[1] |= cab1;
        }
        return 0;

	case I2S_MODE:		/* mono or stereo */
		data = arg;
	    /* For MONO */
		if (data != 2) {
	        ar7240_reg_rmw_set(AR7240_STEREO_CONFIG, MONO);      
        } else {
	        ar7240_reg_rmw_clear(AR7240_STEREO_CONFIG, MONO);      
        }
		return 0;

    case I2S_MCLK:       /* Master clock is MCLK_IN or divided audio PLL */
		data = arg;
        if (data) {
            ar7240_reg_wr(AUDIO_PLL, AUDIO_PLL_RESET); /* Reset Audio PLL */
            ar7240_reg_rmw_set(AR7240_STEREO_CONFIG, AR7240_STEREO_CONFIG_I2S_MCLK_SEL);
        } else {
            ar7240_reg_rmw_clear(AR7240_STEREO_CONFIG, AR7240_STEREO_CONFIG_I2S_MCLK_SEL);
		}
		return 0;

	case I2S_COUNT:
		data = arg;
		return 0;

	default:
		return -ENOTSUPP;
	}


}

irqreturn_t ar7240_i2s_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	uint32_t r;

	r = ar7240_reg_rd(MBOX_INT_STATUS);

#if AOW_AUDIO_SYNC    
    {
      ar7240_i2s_softc_t *sc = &sc_buf_var;
      if (r & MBOX0_RX_DMA_COMPLETE) {
          /* 
           * Calling the tasklet directly, need to profile
           * the delay and study the behaviour of the tasklet
           *
           * I2S_SCHEDULE_TQUEUE(&sc->i2s_tq, NULL);
           *
           */
          if (I2S_OPENED(sc))
              i2s_rx_tasklet(&sinfo);
      }
    }
#endif  /* AOW_AUDIO_SYNC */    
    if(r & RX_UNDERFLOW)
        stats.rx_underflow++;

	/* Ack the interrupts */
	ar7240_reg_wr(MBOX_INT_STATUS, r);

	return IRQ_HANDLED;
}

void ar7240_i2sound_i2slink_on(int master)
{
    /* Clear all resets */
#ifndef I2S_AOW_ENAB    
     ar7240_reg_wr(RST_RESET, 0);
#endif  // I2S_AOW_ENAB     
    udelay(500);

    /* Set GPIO I2S Enables */
    /*
     * 0x3c000001 should be set to GPIO for MIC and I2S
     * 0x7c000001 should be set to GPIO for SPDIF
     */
    ar7240_reg_rmw_set(AR7240_GPIO_FUNCTIONS,
        (AR7240_GPIO_FUNCTION_SPDIF_EN |
        AR7240_GPIO_FUNCTION_I2S_MCKEN | AR7240_GPIO_FUNCTION_I2S0_EN));

    /*
     * GPIO 13 and GPIO 14 are used for I2S logic.
     * So disabling LED on these pins.
     */
    ar7240_reg_rmw_clear(AR7240_GPIO_FUNCTIONS,
        (AR7240_GPIO_FUNCTION_ETH_SWITCH_LED1_EN |
        AR7240_GPIO_FUNCTION_ETH_SWITCH_LED0_EN));

    ar7240_reg_rmw_set(AR7240_GPIO_FUNCTION_2,
        (AR7240_GPIO_FUNCTION_2_EN_I2WS_ON_0 |
        AR7240_GPIO_FUNCTION_2_EN_I2SCK_ON_1 |
        AR7240_GPIO_FUNCTION_2_I2S_ON_LED));

    /*
     * AR7240_STEREO_CONFIG should carry 0x201302 for MIC and I2S
     * AR7240_STEREO_CONFIG should carry 0xa01302 for SPDIF
     */
    ar7240_reg_wr(AR7240_STEREO_CONFIG,
       (AR7240_STEREO_CONFIG_PCM_SWAP|AR7240_STEREO_CONFIG_SPDIF_ENABLE |
        AR7240_STEREO_CONFIG_RESET|
        AR7240_STEREO_CONFIG_ENABLE |
        AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_16B) |
        AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
        AR7240_STEREO_CONFIG_MASTER |
        AR7240_STEREO_CONFIG_PSEDGE(2)));
    udelay(100);
    ar7240_reg_rmw_clear(AR7240_STEREO_CONFIG, AR7240_STEREO_CONFIG_RESET);
}

void ar7240_i2sound_request_dma_channel(int mode)
{
	ar7240_reg_wr(MBOX_DMA_POLICY,
		      ((6 << 16) | (6 << 12) | (6 << 8) | (6 << 4) | 0xa));
}

void ar7240_i2sound_dma_desc(unsigned long desc_buf_p, int mode)
{
	/*
	 * Program the device to generate interrupts
	 * RX_DMA_COMPLETE for mbox 0
	 */
	if (mode) {
		ar7240_reg_wr(MBOX0_DMA_TX_DESCRIPTOR_BASE, desc_buf_p);
	} else {
		ar7240_reg_wr(MBOX0_DMA_RX_DESCRIPTOR_BASE, desc_buf_p);
	}
}

void ar7240_i2sound_dma_start(int mode)
{
	/*
	 * Start
	 */
	if (mode) {
		ar7240_reg_wr(MBOX0_DMA_TX_CONTROL, START);
	} else {
		ar7240_reg_wr(MBOX0_DMA_RX_CONTROL, START);
	}
}
EXPORT_SYMBOL(ar7240_i2sound_dma_start);

void ar7240_i2sound_dma_pause(int mode)
{
        /*
         * Pause
         */
        if (mode) {
                ar7240_reg_wr(MBOX0_DMA_TX_CONTROL, PAUSE);
        } else {
                ar7240_reg_wr(MBOX0_DMA_RX_CONTROL, PAUSE);
        }
}
EXPORT_SYMBOL(ar7240_i2sound_dma_pause);

void ar7240_i2sound_dma_resume(int mode)
{
        /*
         * Resume
         */
        if (mode) {
                ar7240_reg_wr(MBOX0_DMA_TX_CONTROL, RESUME);
        } else {
                ar7240_reg_wr(MBOX0_DMA_RX_CONTROL, RESUME);
        }
}
EXPORT_SYMBOL(ar7240_i2sound_dma_resume);

void ar7240_i2s_clk(unsigned long frac, unsigned long pll)
{
    /*
     * Tick...Tick...Tick
     */
    ar7240_reg_wr(FRAC_FREQ, frac);
    ar7240_reg_wr(AUDIO_PLL, pll);
}
EXPORT_SYMBOL(ar7240_i2s_clk);

loff_t ar7240_i2s_llseek(struct file *filp, loff_t off, int whence)
{
	printk(KERN_CRIT "llseek\n");
	return off;
}


struct file_operations ar7240_i2s_fops = {
	.owner = THIS_MODULE,
	.llseek = ar7240_i2s_llseek,
	.read = ar7240_i2s_read,
	.write = ar9100_i2s_write,
	.ioctl = ar7240_i2s_ioctl,
	.open = ar7240_i2s_open,
	.release = ar7240_i2s_close,
};

void ar7240_i2s_cleanup_module(void)
{
	ar7240_i2s_softc_t *sc = &sc_buf_var;

	printk(KERN_CRIT "unregister\n");

	free_irq(sc->sc_irq, NULL);
	unregister_chrdev(ar7240_i2s_major, "ath_i2s");
}

int ar7240_i2s_init_module(void)
{
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	int result = -1;
	int master = 1;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (ar7240_i2s_major) {
		result =
		    register_chrdev(ar7240_i2s_major, "ath_i2s",
				    &ar7240_i2s_fops);
	}
	if (result < 0) {
		printk(KERN_WARNING "ar7240_i2s: can't get major %d\n",
		       ar7240_i2s_major);
		return result;
	}

	sc->sc_irq = AR7240_MISC_IRQ_DMA;

	/* Establish ISR would take care of enabling the interrupt */
	result = request_irq(sc->sc_irq, ar7240_i2s_intr, AR7240_IRQF_DISABLED,
			     "ar7240_i2s", NULL);
	if (result) {
		printk(KERN_INFO
		       "i2s: can't get assigned irq %d returns %d\n",
		       sc->sc_irq, result);
	}

	ar7240_i2sound_i2slink_on(master);

#if AOW_AUDIO_SYNC    
    init_sync_buffers(&sinfo);
    /* initialize the tasklet */
    I2S_INIT_TQUEUE(&sc->i2s_tq, i2s_rx_tasklet, &sinfo);
#endif  /* AOW_AUDIO_SYNC */

	I2S_LOCK_INIT(&sc_buf_var);

	return 0;		/* succeed */
}

module_init(ar7240_i2s_init_module);
module_exit(ar7240_i2s_cleanup_module);
