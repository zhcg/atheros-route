#include <dv_dbg.h>
#include <asm/addrspace.h>
#include <ar7240_soc.h>
#include <fwd.h>
#include <wasp_api.h>
#include <dma_lib.h>

_A_wasp_indirection_table_t _indir_tbl __attribute__((__section__(".indir_tbl.data")));

static void module_install()
{
    UART_MODULE_INSTALL();
    MEM_MODULE_INSTALL();
    ROM_PATCH_MODULE_INSTALL();
    OTP_MODULE_INSTALL();
    PRINT_MODULE_INSTALL();
    USB_MODULE_INSTALL();
    CLOCK_MODULE_INSTALL();
    MDIO_MODULE_INSTALL();
    HIF_MODULE_INSTALL();
    NAND_MODULE_INSTALL();
    ALLOCRAM_MODULE_INSTALL();
    VDESC_MODULE_INSTALL();
    VBUF_MODULE_INSTALL();
    DMA_LIB_MODULE_INSTALL();
    DMA_ENGINE_MODULE_INSTALL();
    PCI_API_MODULE_INSTALL();
    GMAC_MODULE_INSTALL();
}

static void module_init()
{
    A_CLOCK_INIT();
    A_UART_INIT();
    A_PRINTF_INIT();
    A_ROMP_INIT();
    A_OTP_INIT();
}

void main(void)
{
    A_HOSTIF hif;

    DV_DBG_RECORD_LOCATION(MAIN_C); // Location Pointer

    /* Install all patchable modules */
    module_install();

    /* Subsystems initialization */
    module_init();

#if 0
    A_UART_PUTS("Type Z to skip romp download\n");
    if('Z'!=A_UART_GETC())
#endif
    A_ROMP_DOWNLOAD();          /* patch the rom*/

    DV_DBG_RECORD_LOCATION(MAIN_C); // Location Pointer

    hif = A_FIND_HIF();

#define WASP_BOOTROM_VER	"1.3"

    A_UART_PUTS("WASP BootROM Ver. " WASP_BOOTROM_VER "\n");

    if (hif == HIF_GMAC) {
        mdio_bw_exec_t fw_bw_state;

        DV_DBG_RECORD_LOCATION(MAIN_C); // Location Pointer

        A_UART_PUTS("GMAC start\n");

        /* here we should complete the entire firmware download stuff
           and set the proper gmac stuff here, so that every thing goes
           fine
        */
        A_MDIO_INIT();

        A_UART_PUTS("ROM>:mdio download ready\n");

        A_MEMSET(&fw_bw_state, 0, sizeof(fw_bw_state));
        if (A_MDIO_DOWNLOAD_FW(&fw_bw_state) == 0) {
            /*
             * once completed the entire transfer, try executing
             * the  code from specified address
             * Reset mdio_boot_enable to zero, so that, when\
             * RAM code ( target firmware ) runs, that should not
             * call mdio download agin, instead, real gmac driver
             * should work.
             */
            if (fw_bw_state.exec_address) {
                call_fw(HIF_GMAC, fw_bw_state.exec_address);
            }
            A_UART_PUTS("* * * Return back from f/w * * *\n");
        }
#if 0
        GMAC_fw_download();
#endif
    } else if (hif == HIF_USB) {
	u32		i;

        DV_DBG_RECORD_LOCATION(MAIN_C); // Location Pointer

        A_UART_PUTS("NOW INIT USB\n");

	/* USB Phy Internal POR delay is 20 msec. */
	for (i = 0; i < 100; i++)
		udelay(100);

	ar7240_reg_rmw_set(AR7240_RESET, 0x80000);

#if !CONFIG_WASP_EMU
	i = ar7240_reg_rd(WASP_BOOTSTRAP_REG);
	A_PRINTF("before = 0x%x\n", i);

	i &= ~0x30;
	A_PRINTF("after = 0x%x\n", i);

	ar7240_reg_wr(WASP_BOOTSTRAP_REG, i);	// Supercore shutdown

	i = ar7240_reg_rd(WASP_BOOTSTRAP_REG);
	A_PRINTF("after shutdown = 0x%x\n", i);

	ar7240_reg_wr(0x18116cc0, 0x1061060e);	// RC Phy shutdown
	ar7240_reg_wr(0x18116cc8, 0x780c);	// RC Phy shutdown
	ar7240_reg_wr(0x18116d00, 0x1061060e);	// EP Phy shutdown
	ar7240_reg_wr(0x18116d08, 0x5c0c);	// EP Phy shutdown
#endif

        A_USB_INIT();	// USB_Init_device
        while(1)
        {
            //Query Interrupt
            A_USB_HANDLE_INTERRUPT(); // USB_Handle_Interrupt
        }
    } else if (hif == HIF_NAND) {
        DV_DBG_RECORD_LOCATION(MAIN_C); // Location Pointer
        A_UART_PUTS("Nand Flash init\n");
        A_NAND_INIT();
        A_NAND_LOAD_FW();
    } else if (hif == HIF_PCIE) {
#define ALLOCRAM_START	0xbd002000
#define ALLOCRAM_SIZE	(8*1024)

        DV_DBG_RECORD_LOCATION(MAIN_C); // Location Pointer
        A_UART_PUTS("\n------------------\nPCIe init\n");
        A_ALLOCRAM_INIT(ALLOCRAM_START, ALLOCRAM_SIZE);
        hif_pci_module_install(&_A_WASP_INDIRECTION_TABLE->hif);

#define MAX_BUF_NUM	50
#define MAX_TX_DESC_NUM	40
#define MAX_RX_DESC_NUM	10

        VBUF_init(MAX_BUF_NUM);
        VDESC_init(MAX_TX_DESC_NUM + MAX_RX_DESC_NUM);


        /* set pci conf(); // do we need that??? */
        fwd_init();
        hif_pci_boot_init();	//	__pci_boot_init
        A_UART_PUTS("wait for download\n");

        while(1) {
            HIF_isr_handler(NULL); //	__pci_isr_handler
        }
    } else if (hif == HIF_S27) {
        A_ALLOCRAM_INIT(ALLOCRAM_START, ALLOCRAM_SIZE);
        A_UART_PUTS("\n------------------\nS27 init\n");
        GMAC_INIT();
        GMAC_MAC_SETUP();
        GMAC_PHY_SETUP();
	GMAC_FWD_MAIN();
    }
}

