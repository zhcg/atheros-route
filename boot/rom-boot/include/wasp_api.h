/*
 * @File: wasp_api.h
 *
 * @Abstract: Honret BOOT api
 *
 * @Notes:
 *
 * Copyright (c) 2009 Atheros Communications Inc.
 * All rights reserved.
 *
 */

#ifndef _WASP_API_H
#define _WASP_API_H

typedef signed char		A_CHAR;
typedef signed char		A_INT8;
typedef unsigned char		A_UINT8;
typedef unsigned char		A_UCHAR;
typedef short			A_INT16;
typedef unsigned short		A_UINT16;
typedef int			A_INT32;
typedef unsigned int		A_UINT32;
typedef long long		A_INT64;
typedef unsigned long long	A_UINT64;
typedef int			A_BOOL;
typedef unsigned int		ULONG;
typedef ULONG			A_ULONG;
typedef A_ULONG			A_ADDR;

#include <mem_api.h>
#include <uart_api.h>
#include <romp_api.h>
#include <printf_api.h>
#include <otp_api.h>
#include <usb_api.h>
#include <dt_defs.h>
#include <time.h>
#include <clock_api.h>
#include <exception_api.h>
#include <vbuf_api.h>
#include <hif_api.h>
#include <mdio_api.h>
#include <nand_api.h>
#include <dma_engine_api.h>
#include <dma_lib.h>
#include <allocram_api.h>
#include <hif_pci.h>
#include <gmac_api.h>


#define A_INDIR(sym)   _A_WASP_INDIRECTION_TABLE->sym

#define UART_MODULE_INSTALL()                 uart_module_install(&_A_WASP_INDIRECTION_TABLE->uart)
#define A_UART_INIT()                         A_INDIR(uart._uart_init())
#define A_UART_GETC()                         A_INDIR(uart._uart_getc())
#if 1
#define A_UART_PUTC(c)                        A_INDIR(uart._uart_putc(c))
#define A_UART_PUTS(s)                        A_INDIR(uart._uart_puts(s))
#define A_UART_PUTU8(u8)                      A_INDIR(uart._uart_putu8(u8))
#define A_UART_PUTU32(u32)                    A_INDIR(uart._uart_putu32(u32))
#else
#define A_UART_PUTC(c)
#define A_UART_PUTS(s)
#define A_UART_PUTU8(u8)
#define A_UART_PUTU32(u32)
#endif

/* memory module install */
#define MEM_MODULE_INSTALL()                  mem_module_install(&_A_WASP_INDIRECTION_TABLE->mem)
#define A_MEMSET(_ptr, _c, _cnt)              A_INDIR(mem._memset(_ptr, _c, _cnt))
#define A_MEMCPY(_dest, _src, _cnt)           A_INDIR(mem._memcpy(_dest, _src, _cnt))
#define A_MEMCMP(_s1, _s2, _cnt)              A_INDIR(mem._memcmp(_s1, _s2, _cnt))
#define A_MEMZERO(_ptr, _cnt)                 A_MEMSET(_ptr, 0, _cnt)

#define ARENA_SZ_DEFAULT 12000

#define alloc_arena_start _end
#define A_ALLOCRAM_INIT(arena_start, arena_size)				\
do {										\
	extern unsigned int alloc_arena_start;					\
	void *astart;								\
	int asize;								\
	astart = (arena_start) ? (void *)(arena_start) : &alloc_arena_start;	\
	asize = (arena_size) ? (arena_size) : (ARENA_SZ_DEFAULT);		\
	A_INDIR(api.cmnos_allocram_init((astart), (asize)));			\
} while (0)

extern void cmnos_allocram_module_install(struct allocram_api *);

#define ALLOCRAM_MODULE_INSTALL()             cmnos_allocram_module_install(&_A_WASP_INDIRECTION_TABLE->api)
#define A_ALLOCRAM(n)                         A_INDIR(api.cmnos_allocram(0, (n), __func__, __LINE__))
#define A_ALLOCRAM_DEBUG()                    A_INDIR(api.cmnos_allocram_debug())

/* rom patch module install */
#define ROM_PATCH_MODULE_INSTALL()            romp_module_install(&_A_WASP_INDIRECTION_TABLE->romp)
#define A_ROMP_INIT()                         A_INDIR(romp._romp_init())
#define A_ROMP_DOWNLOAD(x)                    A_INDIR(romp._romp_download(x))
#define A_ROMP_DECODE(x)                      A_INDIR(romp._romp_decode(x))
#define A_ROMP_INSTALL()                      A_INDIR(romp._romp_install())

/* print module install */
#define PRINT_MODULE_INSTALL()                printf_module_install(&_A_WASP_INDIRECTION_TABLE->print)
#define A_PRINTF_INIT()                       A_INDIR(print._printf_init())
#define A_PRINTF                              A_INDIR(print._printf)

/* OTP module install */
#define OTP_MODULE_INSTALL()                  otp_module_install(&_A_WASP_INDIRECTION_TABLE->otp)
#define A_OTP_INIT()                          A_INDIR(otp._otp_init())
#define A_OTP_READ(x)                         A_INDIR(otp._otp_read(x))
#define A_OTP_WRITE(x, y)                     A_INDIR(otp._otp_write(x, y))
#define A_OTP_READ_USB()                      A_INDIR(otp._otp_read_usb())
#define A_OTP_READ_PATCH(x)                   A_INDIR(otp._otp_read_patch(x))
#define A_OTP_READ_MDIO_PADDR()               A_INDIR(otp._otp_read_mdio_paddr())
#define A_OTP_READ_USB_BIAS()                 A_INDIR(otp._otp_read_usb_bias())
#define A_OTP_COPY(x, y, z)                   A_INDIR(otp._otp_copy(x, y, z))
#define A_OTP_GET_NAND_TABLE(x, y)            A_INDIR(otp._otp_get_nand_table(x, y))
#define A_CHKSUM(x, y)                        A_INDIR(otp._otp_chksum(x, y))

/* USB module install */
#define USB_MODULE_INSTALL()                  usb_module_install(&_A_WASP_INDIRECTION_TABLE->usb)
#define A_USB_INIT()                          A_INDIR(usb._usb_init())
#define A_USB_INIT_STAT()                     A_INDIR(usb._usb_init_stat())
#define A_USB_INIT_ENDPT()                    A_INDIR(usb._usb_init_endpt())
#define A_USB_SETUP_DESCRIPTOR()              A_INDIR(usb._usb_setup_descriptor())
#define A_USB_HANDLE_INTERRUPT()              A_INDIR(usb._usb_handle_interrupt())
#define A_USB_HANDLE_VENDOR_REQUEST(x)        A_INDIR(usb._usb_handle_vendor_request(x))
#define A_USB_HANDLE_ERROR(x)                 A_INDIR(usb._usb_handle_error(x))
#define A_USB_HANDLE_SETUP()                  A_INDIR(usb._usb_handle_setup())
#define A_USB_GET_STATUS(x)                   A_INDIR(usb._usb_get_status(x))
#define A_USB_CLEAR_FEATURE(x)                A_INDIR(usb._usb_clear_feature(x))
#define A_USB_SET_FEATURE(x)                  A_INDIR(usb._usb_set_feature(x))
#define A_USB_SET_ADDRESS(x)                  A_INDIR(usb._usb_set_address(x))
#define A_USB_GET_DESCRIPTOR(x)               A_INDIR(usb._usb_get_descriptor(x))
#define A_USB_SET_DESCRIPTOR(x)               A_INDIR(usb._usb_set_descriptor(x))
#define A_USB_GET_CONFIGURATION()             A_INDIR(usb._usb_get_configuration())
#define A_USB_SET_CONFIGURATION(x)            A_INDIR(usb._usb_set_configuration(x))
#define A_USB_GET_INTERFACE(x)                A_INDIR(usb._usb_get_interface(x))
#define A_USB_SET_INTERFACE(x)                A_INDIR(usb._usb_set_interface(x))
#define A_USB_READ_USB_CONFIG()               A_INDIR(usb._usb_read_usb_config())
#define A_USB_READ_OTP_DATA(x)                A_INDIR(usb._usb_read_otp_data(x))
#define A_USB_DO_ENDPT0_RX(x)                 A_INDIR(usb._usb_do_endpt0_rx(x))
#define A_USB_DO_ENDPT0_TX(x,y)               A_INDIR(usb._usb_do_endpt0_tx(x,y))
#define A_USB_JUMP_TO_FW(x,y)                 A_INDIR(usb._usb_jump_to_fw(x,y))
#define A_USB_WATCHDOG_RESET_NOTIFY()         A_INDIR(usb._usb_watchdog_reset_notify())
#define A_USB_INIT_BY_STARTTYPE()             A_INDIR(usb._usb_init_by_starttype())
#define A_USB_CONFIG_PHY_BIAS()	              A_INDIR(usb._usb_config_phy_bias())
#define A_USB_INIT_COLD_START()               A_INDIR(usb._usb_init_cold_start())
#define A_USB_INIT_WARM_START()               A_INDIR(usb._usb_init_warm_start())
#define A_USB_INIT_WATCHDOG_RESET()           A_INDIR(usb._usb_init_watchdog_reset())


/* Clock module install */
#define CLOCK_MODULE_INSTALL()                clock_module_install(&_A_WASP_INDIRECTION_TABLE->clock)
#define A_CLOCK_INIT()                        A_INDIR(clock._clock_init())

/* MDIO module install */
#define MDIO_MODULE_INSTALL()                 mdio_module_install(&_A_WASP_INDIRECTION_TABLE->mdio)
#define A_MDIO_INIT()                         A_INDIR(mdio._mdio_init())
#define A_MDIO_DOWNLOAD_FW(x)                 A_INDIR(mdio._mdio_download_fw(x))
#define A_MDIO_WAIT_LOCK()                    A_INDIR(mdio._mdio_wait_lock())
#define A_MDIO_RELEASE_LOCK(x)                A_INDIR(mdio._mdio_release_lock(x))
#define A_MDIO_READ_BLOCK(x,y)                A_INDIR(mdio._mdio_read_block(x,y))
#define A_MDIO_COPY_BYTES(x,y)                A_INDIR(mdio._mdio_copy_bytes(x,y))
#define A_MDIO_COMP_CKSUM(x,y)                A_INDIR(mdio._mdio_comp_cksum(x,y))

/* HIF module install */
#define HIF_MODULE_INSTALL()                  hif_module_install(&_A_WASP_INDIRECTION_TABLE->hif)
#define A_FIND_HIF()                          A_INDIR(hif._find_hif())
#define HIF_init(pConfig)                     A_INDIR(hif._init(pConfig))
#define HIF_shutdown(h)                       A_INDIR(hif._shutdown(h))
#define HIF_register_callback(h, pConfig)     A_INDIR(hif._register_callback(h, pConfig))
#define HIF_start(h)                          A_INDIR(hif._start(h))
#define HIF_config_pipe(h, pipe, desc_list)   A_INDIR(hif._config_pipe(h, pipe, desc_list))
#define HIF_send_buffer(h, pipe, buf)         A_INDIR(hif._send_buffer(h, pipe, buf))
#define HIF_return_recv_buf(h, pipe, buf)     A_INDIR(hif._return_recv_buf(h, pipe, buf))
#define HIF_isr_handler(h)                    A_INDIR(hif._isr_handler(h))
#define HIF_is_pipe_supported(h, pipe)        A_INDIR(hif._is_pipe_supported(h, pipe))
#define HIF_get_max_msg_len(h, pipe)          A_INDIR(hif._get_max_msg_len(h, pipe))
#define HIF_get_reserved_headroom(h)          A_INDIR(hif._get_reserved_headroom(h))
#define HIF_get_default_pipe(h,u,d)           A_INDIR(hif._get_default_pipe(h,u,d))


/* NAND Flash module install */
#define NAND_MODULE_INSTALL()                 nand_module_install(&_A_WASP_INDIRECTION_TABLE->nand)
#define A_NAND_INIT()                         A_INDIR(nand._nand_init())
#define A_NAND_LOAD_FW()                      A_INDIR(nand._nand_load_fw())
#define A_NAND_READ(a, b, c)                  A_INDIR(nand._nand_read(a, b, c))
#define A_NAND_READ_ID()                      A_INDIR(nand._nand_read_id())
#define A_NAND_PARAM_PAGE(a, b)               A_INDIR(nand._nand_param_page(a, b))
#define A_NAND_STATUS()                       A_INDIR(nand._nand_status())
#define A_NAND_GET_ENTRY(a, b, c, d)          A_INDIR(nand._nand_get_entry(a, b, c, d))

/* DMA Engine Interface */
#define DMA_ENGINE_MODULE_INSTALL()                 dma_engine_module_install(&_A_WASP_INDIRECTION_TABLE->dma_engine);
#define DMA_Engine_init()                           A_INDIR(dma_engine._init())
#define DMA_Engine_config_rx_queue(q, nDesc, size)  A_INDIR(dma_engine._config_rx_queue(q, nDesc, size))
#define DMA_Engine_xmit_buf(q, buf)                 A_INDIR(dma_engine._xmit_buf(q, buf))
#define DMA_Engine_flush_xmit(q)                    A_INDIR(dma_engine._flush_xmit(q))
#define DMA_Engine_reap_recv_buf(q)                 A_INDIR(dma_engine._reap_recv_buf(q))
#define DMA_Engine_return_recv_buf(q,buf)           A_INDIR(dma_engine._return_recv_buf(q, buf))
#define DMA_Engine_reap_xmited_buf(q)               A_INDIR(dma_engine._reap_xmited_buf(q))
#define DMA_Engine_swap_data(desc)                  A_INDIR(dma_engine._swap_data(desc))
#define DMA_Engine_init_rx_queue(q)                 A_INDIR(dma_engine._init_rx_queue(q))
#define DMA_Engine_init_tx_queue(q)                 A_INDIR(dma_engine._init_tx_queue(q))
#define DMA_Engine_has_compl_packets(q)             A_INDIR(dma_engine._has_compl_packets(q))
#define DMA_Engine_desc_dump(q)                     A_INDIR(dma_engine._desc_dump(q))
#define DMA_Engine_get_packet(q)                    A_INDIR(dma_engine._get_packet(q))
#define DMA_Engine_reclaim_packet(q,desc)           A_INDIR(dma_engine._reclaim_packet(q,desc))
#define DMA_Engine_put_packet(q,desc)               A_INDIR(dma_engine._put_packet(q,desc))

/*DMA Library support for GMAC & PCI(E)*/
#define DMA_LIB_MODULE_INSTALL()                    dma_lib_module_install(&_A_WASP_INDIRECTION_TABLE->dma_lib)
#define dma_lib_tx_init(eng_no, if_type)            A_INDIR(dma_lib.tx_init(eng_no, if_type))
#define dma_lib_rx_init(eng_no, if_type)            A_INDIR(dma_lib.rx_init(eng_no, if_type))
#define dma_lib_rx_config(eng_no, desc, gran)       A_INDIR(dma_lib.rx_config(eng_no, desc, gran))
#define dma_lib_tx_start(eng_no)                    A_INDIR(dma_lib.tx_start(eng_no))
#define dma_lib_rx_start(eng_no)                    A_INDIR(dma_lib.rx_start(eng_no))
#define dma_lib_intr_status(if_type)                A_INDIR(dma_lib.intr_status(if_type))
#define dma_lib_hard_xmit(eng_no, buf)              A_INDIR(dma_lib.hard_xmit(eng_no, buf))
#define dma_lib_flush_xmit(eng_no)                  A_INDIR(dma_lib.flush_xmit(eng_no))
#define dma_lib_xmit_done(eng_no)                   A_INDIR(dma_lib.xmit_done(eng_no))
#define dma_lib_reap_xmitted(eng_no)                A_INDIR(dma_lib.reap_xmitted(eng_no))
#define dma_lib_reap_recv(eng_no)                   A_INDIR(dma_lib.reap_recv(eng_no))
#define dma_lib_return_recv(eng_no, buf)            A_INDIR(dma_lib.return_recv(eng_no, buf))
#define dma_lib_recv_pkt(eng_no)                    A_INDIR(dma_lib.recv_pkt(eng_no))

#define VBUF_MODULE_INSTALL()                       vbuf_module_install(&_A_WASP_INDIRECTION_TABLE->vbuf)
#define VBUF_init(nBuf)                             A_INDIR(vbuf._init(nBuf))
#define VBUF_alloc_vbuf()                           A_INDIR(vbuf._alloc_vbuf())
#define VBUF_free_vbuf(buf)                         A_INDIR(vbuf._free_vbuf(buf))

/* VDESC APIs */
#define VDESC_MODULE_INSTALL()                      vdesc_module_install(&_A_WASP_INDIRECTION_TABLE->vdesc)
#define VDESC_init(nDesc)                           A_INDIR(vdesc._init(nDesc))
#define VDESC_alloc_vdesc()                         A_INDIR(vdesc._alloc_vdesc())
#define VDESC_get_hw_desc(desc)                     A_INDIR(vdesc._get_hw_desc(desc))
#define VDESC_swap_vdesc(dst, src)                  A_INDIR(vdesc._swap_vdesc(dst, src))

#define PCI_API_MODULE_INSTALL()            hif_pci_api_install(&_A_WASP_INDIRECTION_TABLE->hif_pci)
#define hif_pci_boot_init()                 A_INDIR(hif_pci.pci_boot_init())
#define hif_pci_init(pConfig)               A_INDIR(hif_pci.pci_init(pConfig))
#define hif_pci_reset()                     A_INDIR(hif_pci.pci_reset())
#define hif_pci_enable()                    A_INDIR(hif_pci.pci_enable())
#define hif_pci_get_pipe(eng)               A_INDIR(hif_pci.pci_get_pipe(eng))
#define hif_pci_get_tx_eng(pipe)            A_INDIR(hif_pci.pci_get_tx_eng(pipe))
#define hif_pci_get_rx_eng(pipe)            A_INDIR(hif_pci.pci_get_rx_eng(pipe))
#define hif_pci_reap_recv(sc, eng_no)       A_INDIR(hif_pci.pci_reap_recv(sc, eng_no))
#define hif_pci_reap_xmitted(sc, eng_no)    A_INDIR(hif_pci.pci_reap_xmitted(sc, eng_no))

#define GMAC_MODULE_INSTALL()		gmac_module_install(&_A_WASP_INDIRECTION_TABLE->gmac)
#define GMAC_MAC_SETUP()		A_INDIR(gmac._gmac_mac_setup)();
#define GMAC_PHY_SETUP()		A_INDIR(gmac._gmac_phy_setup)();
#define GMAC_LOAD_FW()			A_INDIR(gmac._gmac_load_fw)();
#define GMAC_RX_PKT(d, b)		A_INDIR(gmac._gmac_rx_pkt)(d, b);
#define GMAC_RX_DISC(d, b)		A_INDIR(gmac._gmac_rx_disc)(d, b);
#define GMAC_TX_PKT(d)			A_INDIR(gmac._gmac_tx_pkt)(d);
#define GMAC_ALLOC_TX_BUF(b)		A_INDIR(gmac._gmac_alloc_tx_buf)(b);
#define GMAC_DISCOVER()			A_INDIR(gmac._gmac_discover)();
#define GMAC_FWD_MAIN()			A_INDIR(gmac._gmac_fwd_main)();
#define GMAC_INIT()			A_INDIR(gmac._gmac_init)();



hif_handle_t fwd_init();

/*
 * This defines the layout of the indirection table, which
 * is used to access exported APIs of various modules.  The
 * layout is shared across ROM and RAM code.  RAM code may
 * call into ROM and ROM code may call into RAM.  Because
 * of the latter, existing offsets must not change for the
 * lifetime of a revision of ROM; but new members may be
 * added at the end.
 */
typedef struct _A_wasp_indirection_table {
	struct uart_api		uart;
	struct memory_api	mem;
	struct romp_api		romp;
	struct printf_api	print;
	struct otp_api		otp;
	struct usb_api		usb;
	struct clock_api	clock;
	struct mdio_api		mdio;
	struct hif_api		hif;
	struct nand_api		nand;
	struct dma_engine_api	dma_engine;
	struct dma_lib_api	dma_lib;
	struct vdesc_api	vdesc;
	struct vbuf_api		vbuf;
	struct allocram_api	api;
	struct hif_pci_api	hif_pci;
	struct gmac_api		gmac;
} _A_wasp_indirection_table_t;

extern _A_wasp_indirection_table_t _indir_tbl;

#define _A_WASP_INDIRECTION_TABLE_SIZE sizeof(_A_wasp_indirection_table_t)
#define _A_WASP_INDIRECTION_TABLE (&_indir_tbl)

extern void call_fw(int, const unsigned int);
extern __u32 checksum(__u32, __u32);

#endif
