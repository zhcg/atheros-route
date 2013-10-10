#include <wasp_api.h>
#include <asm/addrspace.h>
#include <ar7240_soc.h>

#include <apb_map.h>
#include <dma_lib.h>
#include "hif_pci.h"


/**************************Constants******************************/

/*************************Data types*******************************/
enum pci_regs{
	WASP_REG_AHB_RESET	= 0xb806001c,
	WASP_REG_MISC2_RESET	= 0xb80600bc,
	WASP_PCIEP_VD		= 0xb8230000,
};

#define PCI_AHB_RESET_DMA           (1 << 29)
#define PCI_AHB_RESET_DMA_HST_RAW   (1 << 31)
/* Will be asserted on reset and cleared on s/w read */
#define PCI_AHB_RESET_DMA_HST       (1 << 19)
#define PCI_MISC2_RST_CFG_DONE      (1 <<  0)

/*********************************softC************************/

#define ret_pkt         sw.send_buf_done
#define indicate_pkt    sw.recv_buf
#define htc_ctx         sw.context

/*********************************DEFINES**********************************/
#define  hdl_to_softc(_hdl)     (__pci_softc_t *)(_hdl)

void    __pci_cfg_pipe(hif_handle_t hdl, int pipe, int num_desc);
int     __pci_get_max_msg_len(hif_handle_t hdl, int pipe);
void    __pci_return_recv(hif_handle_t hdl, int pipe, VBUF *buf);
void    __pci_reset(void);
void    __pci_enable(void);


/***********************************Globals********************************/
/**
 * @brief Engines are fixed
 */
__pci_softc_t    pci_sc = {{0}};


/**************************************APIs********************************/

A_UINT32
__pci_reg_read(A_UINT32 addr)
{
    return *((volatile A_UINT32 *)addr);
}

void
__pci_reg_write(A_UINT32 addr, A_UINT32 val)
{
    *((volatile A_UINT32 *)addr) = val;
}

A_UINT8
__pci_get_pipe(dma_engine_t   eng)
{
    switch (eng) {
    case DMA_ENGINE_RX0:
        return HIF_PCI_PIPE_RX0;
    case DMA_ENGINE_RX1:
        return HIF_PCI_PIPE_RX1;
    case DMA_ENGINE_RX2:
        return HIF_PCI_PIPE_RX2;
    case DMA_ENGINE_RX3:
        return HIF_PCI_PIPE_RX3;
    case DMA_ENGINE_TX0:
        return HIF_PCI_PIPE_TX0;
    case DMA_ENGINE_TX1:
        return HIF_PCI_PIPE_TX1;
    default:
        //adf_os_assert(0);
        A_PRINTF("%s(%d): unknown dma engine\n", __func__, __LINE__);
        while (1) { }
    }
}

dma_engine_t
__pci_get_tx_eng(hif_pci_pipe_tx_t  pipe)
{
    switch (pipe) {
    case HIF_PCI_PIPE_TX0:
        return DMA_ENGINE_TX0;

    case HIF_PCI_PIPE_TX1:
        return DMA_ENGINE_TX1;

    default:
        return DMA_ENGINE_MAX;
    }
}
dma_engine_t
__pci_get_rx_eng(hif_pci_pipe_rx_t  pipe)
{
    switch (pipe) {
    case HIF_PCI_PIPE_RX0:
        return DMA_ENGINE_RX0;

    case HIF_PCI_PIPE_RX1:
        return DMA_ENGINE_RX1;

    case HIF_PCI_PIPE_RX2:
        return DMA_ENGINE_RX2;

    case HIF_PCI_PIPE_RX3:
        return DMA_ENGINE_RX3;

    default:
        return DMA_ENGINE_MAX;
    }
}


void
__pci_enable(void)
{
#if 0 // not needed for wasp
    A_UINT32      r_data;
    /**
     * Grant access to the internal memory for PCI DMA
     */

    r_data  = __pci_reg_read(MAG_REG_AHB_ARB);
    r_data |= PCI_AHB_ARB_ENB;
    __pci_reg_write(MAG_REG_AHB_ARB, r_data);
#endif
}

/**
 * @brief PCI reset
 * XXX: Move this to RAM
 */
void
__pci_reset(void)
{
    volatile A_UINT32      r_data;


    r_data = ar7240_reg_rd(RST_REVISION_ID_ADDRESS);
    if ((r_data & (3 << 12)) == (1 << 12)) {
        r_data = ar7240_reg_rd(RST_BOOTSTRAP_ADDRESS);
        if (RST_BOOTSTRAP_RC_SELECT_GET(r_data) == 0) {
            r_data = ar7240_reg_rd(PCIE_PLL_CONFIG_ADDRESS) &
                     (~PCIE_PLL_CONFIG_BYPASS_MASK);
            ar7240_reg_wr(PCIE_PLL_CONFIG_ADDRESS, r_data);
            udelay(1);
        }
    }

    r_data = ar7240_reg_rd(WASP_PCIEP_VD);
    if ((r_data & 0xffff0000) == 0xff1c0000) {
        /*
         * Program the vendor and device ids after reset.
         * In the actual chip this may come from OTP
         * The actual values will be finalised later
         */
        A_PRINTF("Programming PCI Vendor Device id\n");
        ar7240_reg_wr(WASP_PCIEP_VD, 0x0031168c);
    }

    r_data = __pci_reg_read(WASP_REG_MISC2_RESET);
    A_PRINTF("__pci_reset: 0x%x\n", r_data);
    r_data |= PCI_MISC2_RST_CFG_DONE;
    __pci_reg_write(WASP_REG_MISC2_RESET, r_data);

    r_data = __pci_reg_read(WASP_REG_MISC2_RESET);
    A_PRINTF("__pci_reset: 0x%x\n", r_data);
    r_data &= ~PCI_MISC2_RST_CFG_DONE;
    __pci_reg_write(WASP_REG_MISC2_RESET, r_data);
    A_PRINTF("__pci_reset: 0x%x\n", r_data);
    /**
     * Poll until the Host has reset
     */
    A_PRINTF("Waiting for host reset..");
    for (;;) {
        r_data =  __pci_reg_read(WASP_REG_AHB_RESET);

        if ( r_data & PCI_AHB_RESET_DMA_HST_RAW)
            break;
    }
    A_PRINTF("received.\n");

    /**
     * Pull the AHB out of reset
     */

    r_data = __pci_reg_read(WASP_REG_AHB_RESET);
    r_data &= ~PCI_AHB_RESET_DMA;
    __pci_reg_write(WASP_REG_AHB_RESET, r_data);
}
/**
 * @brief Boot init
 */
void
__pci_boot_init(void)
{
    hif_pci_reset();
    hif_pci_enable();

    dma_lib_tx_init(DMA_ENGINE_TX0, DMA_IF_PCI);	// __dma_lib_tx_init
    dma_lib_rx_init(DMA_ENGINE_RX0, DMA_IF_PCI);	// __dma_lib_rx_init

    dma_lib_rx_config(DMA_ENGINE_RX0, PCI_MAX_BOOT_DESC,
                      PCI_MAX_DATA_PKT_LEN);	// __dma_lib_rx_config

}
/**
 * @brief
 *
 * @param pConfig
 *
 * @return hif_handle_t
 */
hif_handle_t
__pci_init(HIF_CONFIG *pConfig)
{
    hif_pci_reset();
    hif_pci_enable();

    /**
     * Initializing the other TX engines
     */
    dma_lib_tx_init(DMA_ENGINE_TX0, DMA_IF_PCI);
    dma_lib_tx_init(DMA_ENGINE_TX1, DMA_IF_PCI);

    /**
     * Initializing the other RX engines
     */
    dma_lib_rx_init(DMA_ENGINE_RX0, DMA_IF_PCI);
    dma_lib_rx_init(DMA_ENGINE_RX1, DMA_IF_PCI);
    dma_lib_rx_init(DMA_ENGINE_RX2, DMA_IF_PCI);
    dma_lib_rx_init(DMA_ENGINE_RX3, DMA_IF_PCI);

    return &pci_sc;
}
/**
 * @brief Configure the receive pipe
 *
 * @param hdl
 * @param pipe
 * @param num_desc
 */
void
__pci_cfg_pipe(hif_handle_t hdl, int pipe, int num_desc)
{
    dma_engine_t    eng;
    A_UINT16        desc_len;

    eng = hif_pci_get_rx_eng(pipe);

    if (eng == DMA_ENGINE_MAX) {
        A_PRINTF("Bad Engine number\n");
        return;
    }

    desc_len = HIF_get_max_msg_len(hdl, pipe);

    dma_lib_rx_config(eng, num_desc, desc_len);
}
/**
 * @brief Start the interface
 *
 * @param hdl
 */
void
__pci_start(hif_handle_t  hdl)
{
    return;
}
/**
 * @brief Register callback of thre HTC
 *
 * @param hdl
 * @param sw
 */
void
__pci_reg_callback(hif_handle_t hdl, HIF_CALLBACK *sw)
{
    __pci_softc_t   *sc = &pci_sc;

    sc->htc_ctx       = sw->context;
    sc->indicate_pkt  = sw->recv_buf;
    sc->ret_pkt       = sw->send_buf_done;
}

/**
 * @brief reap the transmit queue for trasnmitted packets
 *
 * @param sc
 * @param eng_no
 */
void
__pci_reap_xmitted(__pci_softc_t  *sc, dma_engine_t  eng_no)
{
    VBUF *vbuf = NULL;
    A_UINT8     pipe;

    pipe = hif_pci_get_pipe(eng_no);

    vbuf = dma_lib_reap_xmitted(eng_no);

    if ( vbuf )
        sc->ret_pkt(vbuf, sc->htc_ctx);
    else
        A_PRINTF("Empty RX Reap\n");


}

/**
 * @brief reap the receive queue for vbuf's on the specified
 *        engine number
 *
 * @param sc
 * @param eng_no
 */
void
__pci_reap_recv(__pci_softc_t  *sc, dma_engine_t  eng_no)
{
    VBUF   *vbuf = NULL;

    //A_PRINTF("@"); // udelay(1000);

    vbuf = dma_lib_reap_recv(eng_no);

    if(vbuf)
        sc->indicate_pkt(NULL, vbuf, sc->htc_ctx);
    else
        A_PRINTF("Empty TX Reap \n");
}
/**
 * @brief The interrupt handler
 *
 * @param hdl
 */
void
__pci_isr_handler(hif_handle_t hdl)
{
    __pci_softc_t  *sc = &pci_sc;
    A_UINT16      more = 0;

   while ( dma_lib_recv_pkt(DMA_ENGINE_RX3))
       hif_pci_reap_recv(sc, DMA_ENGINE_RX3);

   while ( dma_lib_recv_pkt(DMA_ENGINE_RX2))
       hif_pci_reap_recv(sc, DMA_ENGINE_RX2);

   while ( dma_lib_recv_pkt(DMA_ENGINE_RX1))
       hif_pci_reap_recv(sc, DMA_ENGINE_RX1);

   while ( dma_lib_xmit_done(DMA_ENGINE_TX1))
       hif_pci_reap_xmitted(sc, DMA_ENGINE_TX1);

    do {

        more = 0;

        if( dma_lib_xmit_done(DMA_ENGINE_TX0) ) {
            hif_pci_reap_xmitted(sc,DMA_ENGINE_TX0);
            more = 1;
        }

        if( dma_lib_recv_pkt(DMA_ENGINE_RX0) ) {
            hif_pci_reap_recv(sc, DMA_ENGINE_RX0);
            more = 1;
        }

    } while( more );

}
/**
 * @brief transmit the vbuf from the specified pipe
 *
 * @param hdl
 * @param pipe
 * @param buf
 *
 * @return int
 */
int
__pci_xmit_buf(hif_handle_t hdl, int pipe, VBUF *vbuf)
{
    dma_engine_t   eng;

    eng = hif_pci_get_tx_eng(pipe);

    if (eng == DMA_ENGINE_MAX) {
        A_PRINTF("Invalid Pipe number\n");
        return -1;
    }

    return dma_lib_hard_xmit(eng, vbuf);	// __dma_hard_xmit
}
/**
 * @brief Submit the receive vbuf into the receive queue
 *
 * @param handle
 * @param pipe
 * @param buf
 */
void
__pci_return_recv(hif_handle_t hdl, int pipe, VBUF *buf)
{
    dma_engine_t   eng;

    eng = hif_pci_get_rx_eng(pipe);

    if (eng == DMA_ENGINE_MAX)
        return;

    dma_lib_return_recv(eng, buf);	// __dma_return_recv
}
/**
 * @brief Is this pipe number supported
 *
 * @param handle
 * @param pipe
 *
 * @return int
 */
int
__pci_is_pipe_supported(hif_handle_t hdl, int pipe)
{
    if (pipe >= 0 && pipe <= 4)
        return 1;
    else
        return 0;
}
/**
 * @brief maximum message length this pipe can support
 *
 * @param handle
 * @param pipe
 *
 * @return int
 */
int
__pci_get_max_msg_len(hif_handle_t hdl, int pipe)
{
    if( pipe == HIF_PCI_PIPE_TX0 || pipe == HIF_PCI_PIPE_RX0)
        return PCI_MAX_CMD_PKT_LEN;

    return PCI_MAX_DATA_PKT_LEN;
}
/**
 * @brief return the header room required by this HIF
 *
 * @param hdl
 *
 * @return int
 */
int
__pci_get_reserved_headroom(hif_handle_t  hdl)
{
    return 0;
}
/**
 * @brief Device shutdown, HIF reset required
 *
 * @param hdl
 */
void
__pci_shutdown(hif_handle_t hdl)
{
    return;
}

void
__pci_get_def_pipe(hif_handle_t handle, A_UINT8 *pipe_rx, A_UINT8 *pipe_tx)
{
    *pipe_rx = HIF_PCI_PIPE_RX0;
    *pipe_tx = HIF_PCI_PIPE_TX0;
}
/**
 * @brief This install the API's of the HIF
 *
 * @param apis
 */
void
hif_pci_module_install(struct hif_api *apis)
{
    /* hook in APIs */
    apis->_init         = __pci_init;
    apis->_start        = __pci_start;
    apis->_config_pipe  = __pci_cfg_pipe;
    apis->_isr_handler  = __pci_isr_handler;
    apis->_send_buffer  = __pci_xmit_buf;
    apis->_return_recv_buf       = __pci_return_recv;
    apis->_is_pipe_supported     = __pci_is_pipe_supported;
    apis->_get_max_msg_len       = __pci_get_max_msg_len;
    apis->_register_callback     = __pci_reg_callback;
    apis->_shutdown              = __pci_shutdown;
    apis->_get_reserved_headroom = __pci_get_reserved_headroom;
    apis->_get_default_pipe      = __pci_get_def_pipe;
}

void
hif_pci_api_install(struct hif_pci_api *apis)
{
    apis->pci_boot_init     = __pci_boot_init;
    apis->pci_enable        = __pci_enable;
    apis->pci_init          = __pci_init;
    apis->pci_reap_recv     = __pci_reap_recv;
    apis->pci_reap_xmitted  = __pci_reap_xmitted;
    apis->pci_reset         = __pci_reset;
    apis->pci_get_pipe      = __pci_get_pipe;
    apis->pci_get_rx_eng    = __pci_get_rx_eng;
    apis->pci_get_tx_eng    = __pci_get_tx_eng;
}
