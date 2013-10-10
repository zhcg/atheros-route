#ifndef _NAND_API_H
#define _NAND_API_H

typedef union {
	uint8_t		byte_id[8];

	/*
	 * The following is applicable for Toshiba, Samsung, numonyx and hynix.
	 * have to check for other manufacturers.
	 */
	struct {
		uint8_t	sa1	: 1,	// Serial access time (bit 1)
			org	: 1,	// Organisation
			bs	: 2,	// Block size
			sa0	: 1,	// Serial access time (bit 0)
			ss	: 1,	// Spare size per 512 bytes
			ps	: 2,	// Page Size

			wc	: 1,	// Write Cache
			ilp	: 1,	// Interleaved Programming
			nsp	: 2,	// No. of simult prog pages
			ct	: 2,	// Cell type
			dp	: 2,	// Die/Package

			did,		// Device id
			vid,		// Vendor id

			res1	: 2,	// Reserved
			pls	: 2,	// Plane size
			pn	: 2,	// Plane number
			res2	: 2;	// Reserved
	} id;
} ath_nand_id_t;

typedef struct {
	uint8_t		vid,
			did,
			b3,
			addrcyc,
			pgsz,
			blk,
			spare;
} ath_nand_vend_data_t;

typedef struct {
	uint8_t		vid,
			did,
			b3,
			addrcyc;
} ath_nand_otp_data_t;

typedef enum {
	ATH_NAND_BR,
	ATH_NAND_OTP,
} ath_nand_table_t;

struct nand_api {
	void (*_nand_init)(void);
	void (*_nand_load_fw)(void);
	int (*_nand_read)(unsigned, unsigned, unsigned);
	int (*_nand_param_page)(uint8_t *, unsigned);
	void (*_nand_read_id)(void);
	unsigned (*_nand_status)(void);
	void *(*_nand_get_entry)(ath_nand_id_t *, ath_nand_table_t, void *, int);
};

void
nand_module_install(struct nand_api *api);

#define AR7240_NAND_FLASH_BASE	0x1b000000u
#define AR7240_NF_CMD		(AR7240_NAND_FLASH_BASE + 0x200u)
#define AR7240_NF_CTRL		(AR7240_NAND_FLASH_BASE + 0x204u)
#define AR7240_NF_STATUS	(AR7240_NAND_FLASH_BASE + 0x208u)
#define AR7240_NF_INT_MASK	(AR7240_NAND_FLASH_BASE + 0x20cu)
#define AR7240_NF_INT_STATUS	(AR7240_NAND_FLASH_BASE + 0x210u)
#define AR7240_NF_ADDR0_0	(AR7240_NAND_FLASH_BASE + 0x21cu)
#define AR7240_NF_ADDR0_1	(AR7240_NAND_FLASH_BASE + 0x224u)
#define AR7240_NF_DMA_ADDR	(AR7240_NAND_FLASH_BASE + 0x264u)
#define AR7240_NF_DMA_COUNT	(AR7240_NAND_FLASH_BASE + 0x268u)
#define AR7240_NF_DMA_CTRL	(AR7240_NAND_FLASH_BASE + 0x26cu)
#define AR7240_NF_MEM_CTRL	(AR7240_NAND_FLASH_BASE + 0x280u)
#define AR7240_NF_PG_SIZE	(AR7240_NAND_FLASH_BASE + 0x284u)
#define AR7240_NF_RD_STATUS	(AR7240_NAND_FLASH_BASE + 0x288u)
#define AR7240_NF_TIMINGS_ASYN	(AR7240_NAND_FLASH_BASE + 0x290u)

#define CUSTOM_SIZE_EN		0x1	//1 = Enable, 0 = Disable
#define TIMING_ASYN		0x11
#define READ_STATUS_MASK	0xc7
#define READ_STATUS_OK		0xc0

/*
 * Per Spec, This should be in byte no. 101. But because of
 * the byte swapping by the controller during DMA, it gets
 * moved to byte 102.
 */
#define ONFI_NUM_ADDR_CYCLES	102
#define ONFI_PAGE_SIZE		80
#define ONFI_SPARE_SIZE		84
#define ONFI_PAGES_PER_BLOCK	92
#define ONFI_RD_PARAM_PAGE_SZ	128
#define READ_PARAM_STATUS_OK	0x40
#define READ_PARAM_STATUS_MASK	0x41


#define AR7240_NF_CMD_END_INT		(1 << 1)
#define AR7240_NF_STATUS_RETRY		1000
#define ath_nand_get_cmd_end_status(void)	\
	(ar7240_reg_rd(AR7240_NF_INT_STATUS) & AR7240_NF_CMD_END_INT)
#define ath_nand_clear_int_status()	ar7240_reg_wr(AR7240_NF_INT_STATUS, 0)

extern ath_nand_id_t *nand_id;

#endif
