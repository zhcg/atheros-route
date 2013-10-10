#include <asm/addrspace.h>
#include <wasp_api.h>
#include <apb_map.h>
#include <nand_api.h>
#include <include/dv_dbg.h>
#include <ar7240_soc.h>

#define dbg(a, ...)				\
if ((*(unsigned *)0xbd000000) == 0x1234) {	\
	A_PRINTF(a, ## __VA_ARGS__);	\
}

__u32
checksum(__u32 addr, __u32 size)
{
	__u32 *data = (__u32 *) addr;
	__u32 checksum = 0;
	int i;

	for (i = 0; i < size; i += 4) {
		checksum = checksum ^ *data;
		data++;
	}

	return checksum;
}

void call_fw(int intf, const unsigned int ep)
{
	A_PRINTF("Checksum passed for 0x%x\n", ep);
	return;
}

int
nand_block_erase(unsigned addr0, unsigned addr1)
{
	unsigned        rddata;

	ath_nand_clear_int_status();
	ar7240_reg_wr(AR7240_NF_ADDR0_0, addr0);
	ar7240_reg_wr(AR7240_NF_ADDR0_1, addr1);
	ar7240_reg_wr(AR7240_NF_CMD, 0xd0600e);   // BLOCK ERASE

	while (ath_nand_get_cmd_end_status() == 0);

	rddata = A_NAND_STATUS() & READ_STATUS_MASK;
	if (rddata != READ_STATUS_OK) {
		A_PRINTF("Erase Failed. status = 0x%x", rddata);
		return 1;
	}
	return 0;
}

int
nand_write(unsigned nand_addr, unsigned count, unsigned buf)
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
		ar7240_reg_wr(AR7240_NF_DMA_CTRL, 0x8c);

		ath_nand_clear_int_status();
		// Write PAGE
		ar7240_reg_wr(AR7240_NF_CMD, 0x10804c);
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

#define ATH_FW_SIZE	(256 << 10)

void
nand_burn_fw(void)
{
	unsigned size, ba0, ba1, fw_ram_addr = ATH_FW_ADDR;
	int	i, shift;

	shift = 10 + nand_id->id.ps;

	A_PRINTF("Erase ...\n");
	size = (64 << 10) << nand_id->id.bs;
	for (i = 0; i < ATH_FW_SIZE; i += size) {
		ba0 = ((i >> shift) << 16);
		ba1 = ((i >> (shift + 16)) & 0xf);
		dbg("%s: erase 0x%x 0x%x 0x%x\n", __func__, i, ba0, ba1);
		nand_block_erase(ba0, ba1);
	}

	A_PRINTF("Burn ...\n");
	size = (1 << 10) << nand_id->id.ps;
	for (i = 0; i < ATH_FW_SIZE; i += size, fw_ram_addr += size) {
		ba0 = ((i >> shift) << 16);
		dbg("%s: write 0x%x 0x%x 0x%x\n", __func__, i, size, fw_ram_addr);
		nand_write(ba0, size, fw_ram_addr);
	}
}

_A_wasp_indirection_table_t _indir_tbl __attribute__((__section__(".indir_tbl.data")));

int
otp_get_nand_table(u8 *tbl, u8 len)
{
	A_PRINTF("Skipping OTP read\n");
	return -1;
}

static void module_install()
{
	UART_MODULE_INSTALL();
	MEM_MODULE_INSTALL();
	PRINT_MODULE_INSTALL();
	CLOCK_MODULE_INSTALL();
	NAND_MODULE_INSTALL();
	// Override with dummy function. Nand flash init will check
	// the otp if the device is not available in the table.
	_indir_tbl.otp._otp_get_nand_table = otp_get_nand_table;
}

static void module_init()
{
	A_CLOCK_INIT();
	A_UART_INIT();
	A_PRINTF_INIT();
}

void main(void)
{
	/* Install all patchable modules */
	module_install();

	/* Subsystems initialization */
	module_init();

#define __stringify(a)	#a
#define stringify(a)	__stringify(a)

#define ATH_BOOTROM_VER	"1.0 [" __DATE__ " " __TIME__ "]"

	A_UART_PUTS(	"\n__________________sri____________________\n"
			stringify(CHIP)
			" Nand Burn " ATH_BOOTROM_VER
			"\n_________________________________________\n");

	DV_DBG_RECORD_LOCATION(MAIN_C); // Location Pointer
	A_UART_PUTS("Nand Flash init\n");
	A_NAND_INIT();

	nand_burn_fw();

	A_NAND_LOAD_FW();

	while (1);
}

