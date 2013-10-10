#include <asm/addrspace.h>
#include <wasp_api.h>
#include <nand_api.h>
#include <apb_map.h>
#include <ar7240_soc.h>
#include <include/dv_dbg.h>

uint32_t ath_nand_blk_size[] = { 64 << 10, 128 << 10, 256 << 10, 512 << 10 };
uint32_t ath_nand_page_size[] = { 1 << 10, 2 << 10, 4 << 10, 8 << 10 };

uint32_t ath_nf_ctrl_blk_size[] = { 32 << 10, 64 << 10, 128 << 10, 256 << 10, 0 };
uint32_t ath_nf_ctrl_page_size[] = { 256, 512, 1024, 2048, 4096, 8192, 16384, 0 };

ath_nand_vend_data_t ath_nand_arr[] = {
	{ 0x20, 0xda, 0x10, 5, 3, 1, 1 },	// NU2g3B2D
	{ 0x20, 0xf1, 0x00, 4, 3, 1, 1 },	// NU1g3B2C
	{ 0x20, 0xdc, 0x10, 5, 3, 1, 1 },	// NU4g3B2D
	{ 0x20, 0xd3, 0x10, 5, 4, 1, 1 },	// NU8g3F2A
	{ 0x20, 0xd3, 0x14, 5, 3, 2, 1 },	// NU8g3C2B
	{ 0xad, 0xf1, 0x00, 4, 3, 1, 1 },	// HY1g2b
	{ 0xad, 0xda, 0x10, 5, 3, 1, 1 },	// HY2g2b
	{ 0xec, 0xf1, 0x00, 4, 3, 1, 1 },	// Samsung 3,3V 8-bit [128MB]
	{ 0x98, 0xd1, 0x90, 4, 3, 1, 1 },	// Toshiba
};

ath_nand_otp_data_t ath_nand_otp_arr[OTP_NUM_NAND_ENTRIES];

#define NUM_ARRAY_ENTRIES(a)	(sizeof((a)) / sizeof((a)[0]))
#define NUM_ATH_NAND		NUM_ARRAY_ENTRIES(ath_nand_arr)
#define NUM_ATH_OTP_NAND	NUM_ARRAY_ENTRIES(ath_nand_otp_arr)

static ath_nand_id_t __ath_nand_id;
ath_nand_id_t *nand_id = &__ath_nand_id;
uint32_t nand_dev_ps;

int nand_param_page(uint8_t *, unsigned);

static unsigned
nand_status(void)
{
	unsigned	rddata, i, j, dmastatus;

	rddata = ar7240_reg_rd(AR7240_NF_STATUS);
	for (i = 0; i < AR7240_NF_STATUS_RETRY && rddata != 0xff; i++) {
		udelay(25);
		rddata = ar7240_reg_rd(AR7240_NF_STATUS);
	}

	dmastatus = ar7240_reg_rd(AR7240_NF_DMA_CTRL);
	for (j = 0; j < AR7240_NF_STATUS_RETRY && !(dmastatus & 1); j++) {
		udelay(25);
		dmastatus = ar7240_reg_rd(AR7240_NF_DMA_CTRL);
	}

	if ((i == AR7240_NF_STATUS_RETRY) || (j == AR7240_NF_STATUS_RETRY)) {
		A_PRINTF("NF Status (%u) = 0x%x DMA Ctrl (%u) = 0x%x\n", i, rddata, j, dmastatus);
		//while (1);
		A_NAND_INIT();
		return -1;
	}
	ath_nand_clear_int_status();
	ar7240_reg_wr(AR7240_NF_CMD, 0x07024);    // READ STATUS
	while (ath_nand_get_cmd_end_status() == 0);
	rddata = ar7240_reg_rd(AR7240_NF_RD_STATUS);

	return rddata;
}

void nand_read_id(void)
{
	unsigned int	rddata;

	ath_nand_clear_int_status();
	ar7240_reg_wr(AR7240_NF_DMA_ADDR,
			(unsigned int)PHYSADDR(nand_id));	// DMA Start Addr
	ar7240_reg_wr(AR7240_NF_ADDR0_0, 0x0);			// ADDR0_0 Reg Settings
	ar7240_reg_wr(AR7240_NF_ADDR0_1, 0x0);			// ADDR0_1 Reg Settings
	ar7240_reg_wr(AR7240_NF_DMA_COUNT, sizeof(*nand_id));	// DMA count
	ar7240_reg_wr(AR7240_NF_PG_SIZE, 0x8);			// Custom Page Size
	ar7240_reg_wr(AR7240_NF_DMA_CTRL, 0xcc);		// DMA Control Reg
	ar7240_reg_wr(AR7240_NF_CMD, 0x9061);			// READ ID
	while (ath_nand_get_cmd_end_status() == 0);

	rddata = A_NAND_STATUS();
}


void *
nand_get_entry(ath_nand_id_t *nand_id, ath_nand_table_t type, void *ptr, int count)
{
	int	i;
	ath_nand_otp_data_t	*tbl = ptr;

	for (i = 0; i < count; i++) {
		if ((nand_id->id.vid == tbl->vid) &&
		    (nand_id->id.did == tbl->did) &&
		    (nand_id->byte_id[1] == tbl->b3)) {
			return tbl;
		}
		if (type == ATH_NAND_BR) {
			ptr = ((ath_nand_vend_data_t *)tbl) + 1;
			tbl = ptr;
		} else if (type == ATH_NAND_OTP) {
			tbl ++;
		}
	}

	return NULL;
}


void nand_init(void)
{
	unsigned int	i, rddata, status;
	uint8_t		buf[128];
	ath_nand_vend_data_t	*entry;

	ar7240_reg_rmw_set(ATHR_RESET, RST_RESET_NANDF_RESET_MASK | RST_RESET_ETH_SWITCH_ARESET_MASK);
	udelay(250);

	// Pull Nand Flash and switch analog out of reset
	ar7240_reg_rmw_clear(ATHR_RESET, RST_RESET_ETH_SWITCH_ARESET_MASK);
	udelay(250);

	ar7240_reg_rmw_clear(AR7240_RESET, RST_RESET_NANDF_RESET_MASK);
	udelay(100);

	ar7240_reg_wr(AR7240_NF_INT_MASK, AR7240_NF_CMD_END_INT);
	ath_nand_clear_int_status();

	// TIMINGS_ASYN Reg Settings
	ar7240_reg_wr(AR7240_NF_TIMINGS_ASYN, TIMING_ASYN);
	rddata = ar7240_reg_rd(AR7240_NF_CTRL);

	// Reset Command
	ar7240_reg_wr(AR7240_NF_CMD, 0xff00);
	while (ath_nand_get_cmd_end_status() == 0);
	udelay(1000);

	status = ar7240_reg_rd(AR7240_NF_STATUS);
	for (i = 0; i < AR7240_NF_STATUS_RETRY && status != 0xff; i++) {
		udelay(25);
		status = ar7240_reg_rd(AR7240_NF_STATUS);
	}

	if (i == AR7240_NF_STATUS_RETRY) {
		A_PRINTF("device reset failed\n");
		while(1);
	}

	// Try to see the flash id
	A_NAND_READ_ID();

	if (rddata == 0) {

		nand_dev_ps = (1 << 10) << nand_id->id.ps;		// page size

		ar7240_reg_wr(AR7240_NF_TIMINGS_ASYN, TIMING_ASYN);

		entry = A_NAND_GET_ENTRY(nand_id, ATH_NAND_BR, ath_nand_arr, NUM_ATH_NAND);
		if (entry) {
			// Is it in the list of known devices
			ar7240_reg_wr(AR7240_NF_CTRL,
					(entry->addrcyc) |
					(entry->blk << 6) |
					(entry->pgsz << 8) |
					(CUSTOM_SIZE_EN << 11));
		} else if ((A_OTP_GET_NAND_TABLE((u8 *)ath_nand_otp_arr, sizeof(ath_nand_otp_arr)) == 0) &&
			   (entry = A_NAND_GET_ENTRY(nand_id, ATH_NAND_OTP, ath_nand_otp_arr, NUM_ATH_OTP_NAND))) {
			// Is it available in the OTP
			ar7240_reg_wr(AR7240_NF_CTRL,
					(entry->addrcyc) |
					(entry->blk << 6) |
					(entry->pgsz << 8) |
					(CUSTOM_SIZE_EN << 11));
		} else if (A_NAND_PARAM_PAGE(buf, sizeof(buf)) == 0) {	// ONFI compliant?
			unsigned	ps,	// page size
					ppb;	// pages per block

			rddata = buf[ONFI_NUM_ADDR_CYCLES];
			rddata = ((rddata >> 4) & 0xf) + (rddata & 0xf);

			nand_dev_ps = ps = *(uint32_t *)(&buf[ONFI_PAGE_SIZE]);
			ppb = *(uint32_t *)(&buf[ONFI_PAGES_PER_BLOCK]);

			switch (ps) {
			case 256: ps = 0; break;
			case 512: ps = 1; break;
			case 1024: ps = 2; break;
			case 2048: ps = 3; break;
			case 4096: ps = 4; break;
			case 8192: ps = 5; break;
			case 16384: ps = 6; break;
			default: A_PRINTF("ONFI: Cannot handle ps = %u\n", ps);
				while(1);
			}

			switch (ppb) {
			case 32: ppb = 0; break;
			case 64: ppb = 1; break;
			case 128: ppb = 2; break;
			case 256: ppb = 3; break;
			default: A_PRINTF("ONFI: Cannot handle ppb = %u\n", ppb);
				while(1);
			}

			// Control Reg Setting
			ar7240_reg_wr(AR7240_NF_CTRL,
					(rddata) |
					(ppb << 6) |
					(ps << 8) |
					(CUSTOM_SIZE_EN << 11));
			A_PRINTF("ONFI: Control Setting = 0x%x\n", ar7240_reg_rd(AR7240_NF_CTRL));

		} else {
			unsigned ps, bs;
			DV_DBG_RECORD_LOCATION(NAND_C);
			for (ps = 0; ps < ath_nf_ctrl_page_size[ps]; ps ++) {
				if (ath_nf_ctrl_page_size[ps] == ath_nand_page_size[nand_id->id.ps])
					break;
			}
			for (bs = 0; bs < ath_nf_ctrl_blk_size[bs]; bs ++) {
				if (ath_nf_ctrl_blk_size[bs] == ath_nand_blk_size[nand_id->id.bs])
					break;
			}

			A_PRINTF("Unknown Nand device: 0x%x %x %x %x %x\n",
					nand_id->byte_id[0], nand_id->byte_id[1],
					nand_id->byte_id[2], nand_id->byte_id[3],
					nand_id->byte_id[4]);

			if (ath_nf_ctrl_page_size[ps] == 0 || ath_nf_ctrl_blk_size[bs] == 0) {
				A_PRINTF("Unknown Page size = %u and/or Blk size %u\n",
					ath_nand_page_size[nand_id->id.ps],
					ath_nand_blk_size[nand_id->id.bs]);
				while (1);
			}

			A_PRINTF("Attempting to use unknown device\n");

			ar7240_reg_wr(AR7240_NF_CTRL,
					5 |	// Assume 5 addr cycles...
					(bs << 6) |
					(ps << 8) |
					(CUSTOM_SIZE_EN << 11));
		}
	}
	// NAND Mem Control Reg
	ar7240_reg_wr(AR7240_NF_MEM_CTRL, 0xff00);
}


int
nand_param_page(uint8_t *buf, unsigned count)
{
	unsigned int	tries, rddata;

	for (tries = 3; tries; tries --) {
		//A_PRINTF("%s: reading %p %p\n", __func__, nand_addr, buf);

		// ADDR0_0 Reg Settings
		ar7240_reg_wr(AR7240_NF_ADDR0_0, 0x0);

		// ADDR0_1 Reg Settings
		ar7240_reg_wr(AR7240_NF_ADDR0_1, 0x0);

		// DMA Start Addr
		ar7240_reg_wr(AR7240_NF_DMA_ADDR, PHYSADDR(buf));

		// DMA count
		ar7240_reg_wr(AR7240_NF_DMA_COUNT, count);

		// Custom Page Size
		ar7240_reg_wr(AR7240_NF_PG_SIZE, count);

		// DMA Control Reg
		ar7240_reg_wr(AR7240_NF_DMA_CTRL, 0xcc);

		ath_nand_clear_int_status();
		// READ PARAMETER PAGE
		ar7240_reg_wr(AR7240_NF_CMD, 0xec62);
		while (ath_nand_get_cmd_end_status() == 0);

		rddata = A_NAND_STATUS() & READ_PARAM_STATUS_MASK;
		if (rddata == READ_PARAM_STATUS_OK) {
			break;
		} else {
			A_PRINTF("\nParam Page Failure: 0x%x", rddata);
			DV_DBG_RECORD_LOCATION(NAND_C);
		}
	}

	if ((rddata == READ_PARAM_STATUS_OK) &&
	    (buf[3] == 'O' && buf[2] == 'N' && buf[1] == 'F' && buf[0] == 'I')) {
		DV_DBG_RECORD_LOCATION(NAND_C);
		return 0;
	}

	DV_DBG_RECORD_LOCATION(NAND_C);
	return 1;

}

int
nand_read(unsigned nand_addr, unsigned count, unsigned buf)
{
	unsigned int rddata, repeat = 0;

	//A_PRINTF("%s: reading %p %p\n", __func__, nand_addr, buf);
#define MAX_REPEAT	3

	while (repeat < MAX_REPEAT) {
		repeat ++;

		// ADDR0_0 Reg Settings
		ar7240_reg_wr(AR7240_NF_ADDR0_0, nand_addr);

		// ADDR0_1 Reg Settings
		ar7240_reg_wr(AR7240_NF_ADDR0_1, 0x0);

		// DMA Start Addr
		ar7240_reg_wr(AR7240_NF_DMA_ADDR, PHYSADDR(buf));

		// DMA count
		ar7240_reg_wr(AR7240_NF_DMA_COUNT, count);

		// Custom Page Size
		ar7240_reg_wr(AR7240_NF_PG_SIZE, count);

		// DMA Control Reg
		ar7240_reg_wr(AR7240_NF_DMA_CTRL, 0xcc);

		ath_nand_clear_int_status();
		// READ PAGE
		ar7240_reg_wr(AR7240_NF_CMD, 0x30006a);
		while (ath_nand_get_cmd_end_status() == 0);
		udelay(100);

		rddata = A_NAND_STATUS();
		if ((rddata & READ_STATUS_MASK) != READ_STATUS_OK) {
			A_PRINTF("\nRead Failure: 0x%x", rddata);
		} else {
			break;
		}
	}
	return rddata;
}

void
nand_dump_blk(unsigned fa, unsigned char *b, int count)
{
	int		i, j;

	for (i = 0; i < count; ) {
		A_PRINTF("%p:", i);
		for (j = 0;j < 16 && i < count; j++, i++) {
			A_PRINTF(" %02x", b[i]);
		}
		A_UART_PUTS("\n");
	}
}

/* f/w addr in flash */
#if 0
const uint32_t img_fa[] = { 0x80000000u, 0x80050000u };
#define NUM_FW	(sizeof(img_fa) / sizeof(img_fa[0]))
#else
#define NUM_FW	2
#endif

typedef struct {
	uint32_t	ep,	/* entry point */
			la,	/* load addr */
			sz,	/* firmware size */
			cs;	/* firmware crc checksum */
} nf_fw_hdr_t;

void
nand_load_fw(void)
{
	unsigned	j, i,
			nbs = 65536,	// for nand addr formation
			dbs,
			fa = 0x00000000u;
	nf_fw_hdr_t	hdr, *tmp = (nf_fw_hdr_t *)0xbd004000;

	extern	void	call_fw(int, const unsigned int);
	extern __u32	checksum(__u32, __u32);

	dbs = nand_dev_ps;

	for (j = 0; j < NUM_FW; j++) {

		/*
		 * The reads happen to uncached addresses, to avoid
		 * cache invalidation etc...
		 */
		A_NAND_READ(fa, dbs, (unsigned)tmp);
		hdr = *tmp;

		A_PRINTF("hdr: [0x%x : 0x%x : 0x%x : 0x%x]\n",
				hdr.ep, hdr.la, hdr.sz, hdr.cs);

		A_PRINTF("%s: read %u pages\n", __func__, (hdr.sz / dbs));

		A_MEMCPY((A_UINT8 *)hdr.la, tmp + 1, dbs - sizeof(hdr));

		for (i = dbs, fa += nbs; i < hdr.sz; fa += nbs, i += dbs) {
			A_PRINTF("%s: 0x%x 0x%x 0x%x\n", __func__, fa, dbs, hdr.la + i - sizeof(hdr));
			A_NAND_READ(fa, dbs, hdr.la + i - sizeof(hdr));
		}

		if ((i = checksum(hdr.la, hdr.sz - sizeof(hdr))) != hdr.cs) {
			A_PRINTF("Checksum mismatch. 0x%x != 0x%x\n",
					hdr.cs, i);

			while(1);	// What to do

		} else {
			A_PRINTF("f/w %u read complete, jumping to %p\n",
					j, hdr.ep);
		}

		call_fw(HIF_NAND, hdr.ep);

		A_PRINTF("f/w %u execution complete\n", j);
	}
	while (1);
}

void nand_module_install(struct nand_api *api)
{
	api->_nand_init		= nand_init;
	api->_nand_load_fw	= nand_load_fw;
	api->_nand_read		= nand_read;
	api->_nand_param_page	= nand_param_page;
	api->_nand_read_id	= nand_read_id;
	api->_nand_status	= nand_status;
	api->_nand_get_entry	= nand_get_entry;
}
