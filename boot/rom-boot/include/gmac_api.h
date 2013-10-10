#ifndef _GMAC_API_H
#define _GMAC_API_H

typedef struct {
	volatile
	uint32_t	pkt_start_addr,
			is_empty	:  1,
					:  6,
			more		:  1,
					:  3,
			ftpp_override	:  5,
					:  4,
			pkt_size	: 12,
			next_desc;
} athr_gmac_desc_t;

typedef struct {
	uint8_t			*buf,
				*data;
	athr_gmac_desc_t	*desc;
	uint16_t		buflen,
				datalen;
} athr_gmac_txbuf_t;

struct gmac_api {
	void (*_gmac_init)(void);
	void (*_gmac_mac_setup)(void);
	void (*_gmac_phy_setup)(void);
	void (*_gmac_load_fw)(void);
	void (*_gmac_discover)(void);
	void (*_gmac_fwd_main)(void);
	void (*_gmac_rx_pkt)(athr_gmac_desc_t *, uint8_t *);
	int  (*_gmac_rx_disc)(athr_gmac_desc_t *, uint8_t *);
	void (*_gmac_tx_pkt)(athr_gmac_txbuf_t *);
	void (*_gmac_alloc_tx_buf)(athr_gmac_txbuf_t *);
};

/* Ownership values are diff between Rx and Tx */
#define TX_HW_OWN			0
#define TX_SW_OWN			1

#define RX_HW_OWN			1
#define RX_SW_OWN			0

void gmac_module_install(struct gmac_api *api);

#define ATHR_GMAC_TX_FIFO_LEN		2048
#define AR7240_GE0_BASE			0x19000000  /* 16M */
#define AR7240_GE1_BASE			0x1a000000  /* 16M */

extern uint32_t ge_base[];

#define ATHR_GMAC_CFG1			0x00
#define ATHR_GMAC_CFG2			0x04

#define ATHR_GMAC_CFG1_SOFT_RST		(1 << 31)
#define ATHR_GMAC_CFG1_RX_RST		(1 << 19)
#define ATHR_GMAC_CFG1_TX_RST		(1 << 18)

#define AR7240_RESET_GE0_MAC		(1 << 9)
#define AR7240_RESET_GE1_MAC		(1 << 13)

#define ATHR_RESET			AR7240_RESET

#define ATHR_NUM_TX_DESC		1
#define ATHR_NUM_RX_DESC		1

/* Sufficient for our discover and ack packets */
#define ATHR_TX_BUF_SIZE		64

/*
 * We receive:	hdr		  20
 *		fwd cmd		   8
 *		Load addr	   4	(Only for 1st packet)
 *		Data		1024
 *		Checksum	   4
 *				----
 *				1060	(Maximum)
 */
#define ATHR_RX_BUF_SIZE		1096


#define ATHR_GMAC_CFG1_RX_EN		(1 << 2)
#define ATHR_GMAC_CFG1_TX_EN		(1 << 0)

#define ATHR_GMAC_CFG2_PAD_CRC_EN	(1 << 2)
#define ATHR_GMAC_CFG2_LEN_CHECK	(1 << 4)
//#define ATHR_GMAC_CFG2_LEN_CHECK	(1 << 5)
#define ATHR_GMAC_CFG2_IF_10_100	(1 << 8)
#define ATHR_GMAC_CFG2_IF_1000		(1 << 9)

#define ATHR_GMAC_FIFO_CFG_0		0x48
#define ATHR_GMAC_FIFO_CFG_1		0x4c
#define ATHR_GMAC_FIFO_CFG_2		0x50
#define ATHR_GMAC_FIFO_CFG_3		0x54
#define ATHR_GMAC_FIFO_CFG_4		0x58
#define ATHR_GMAC_FIFO_CFG_5		0x5c


#define ATHR_GMAC_GE0_MAC_ADDR1		0x40
#define ATHR_GMAC_GE0_MAC_ADDR2		0x44

#define ATHR_GMAC_DMA_TX_DESC_Q0	0x184

#define ATHR_GMAC_DMA_TX_CTRL		0x180
#define ATHR_GMAC_DMA_TX_DESC		0x184
#define ATHR_GMAC_DMA_TX_STATUS		0x188
#define ATHR_GMAC_DMA_RX_CTRL		0x18c
#define ATHR_GMAC_DMA_RX_DESC		0x190
#define ATHR_GMAC_DMA_RX_STATUS		0x194
#define ATHR_GMAC_DMA_INTR_MASK		0x198
#define ATHR_GMAC_DMA_INTR		0x19c
#define ATHR_GMAC_DMA_RXFSM		0x1b0
#define ATHR_GMAC_DMA_TXFSM		0x1b4
#define ATHR_GMAC_DMA_XFIFO_DEPTH	0x1a8

#define ATHR_GMAC_CFG1_RX_FCTL		(1 << 5)

#define ATHR_GMAC_MII_MGMT_CFG		0x20


#define ATHR_GMAC_TXE			(1 << 0)
#define ATHR_GMAC_RXE			(1 << 0)

#define	ATHR_BOOTROM_MAC		0

#if (ATHR_BOOTROM_MAC == 0)
#	define	AR7240_RESET_GE_MAC	AR7240_RESET_GE0_MAC
#	define	ATHR_GMAC_CFG2_VAL	( ATHR_GMAC_CFG2_PAD_CRC_EN |	\
					  ATHR_GMAC_CFG2_LEN_CHECK |	\
					  ATHR_GMAC_CFG2_IF_10_100)
#elif (ATHR_BOOTROM_MAC == 1)
#	define	AR7240_RESET_GE_MAC	AR7240_RESET_GE1_MAC
#	define	ATHR_GMAC_CFG2_VAL	( ATHR_GMAC_CFG2_PAD_CRC_EN |	\
					  ATHR_GMAC_CFG2_LEN_CHECK |	\
					  ATHR_GMAC_CFG2_IF_1000)
#else
#	error	"invalid ATHR_BOOTROM_MAC"
#endif

#define athr_gmac_reg_rd(m, r)	({		\
	ar7240_reg_rd(ge_base[m] + (r));	\
})

#define athr_gmac_reg_wr(m, r, v)	({	\
	ar7240_reg_wr((ge_base[m] + (r)), v);	\
})

#define athr_gmac_reg_rmw_set(m, r, v)	({	\
	unsigned int		__w;		\
	__w = athr_gmac_reg_rd(m, r) | (v);	\
	athr_gmac_reg_wr(m, r, __w);		\
})

#define athr_gmac_reg_rmw_clear(m, r, v)	({	\
	unsigned int		__w;			\
	__w = athr_gmac_reg_rd(m, r) | ~(v);		\
	athr_gmac_reg_wr(m, r, __w);			\
})

#endif
