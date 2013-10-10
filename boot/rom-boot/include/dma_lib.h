#ifndef __DMA_LIB_H
#define __DMA_LIB_H


/***********************External***************************/

/**
 * @brief DMA engine numbers, HIF need to map them to there
 *        respective order
 */
typedef enum dma_engine{
	DMA_ENGINE_RX0,
	DMA_ENGINE_RX1,
	DMA_ENGINE_RX2,
	DMA_ENGINE_RX3,
	DMA_ENGINE_TX0,
	DMA_ENGINE_TX1,
	DMA_ENGINE_MAX
}dma_engine_t;

/**
 * @brief Interface type, each HIF should call with its own interface type
 */
typedef enum dma_iftype{
	DMA_IF_GMAC	= 0x0,/* GMAC */
	DMA_IF_PCI	= 0x1,/*PCI */
	DMA_IF_PCIE	= 0x2 /*PCI Express */
}dma_iftype_t;


struct dma_lib_api{
	a_status_t	(*tx_init)(dma_engine_t, dma_iftype_t);
	void		(*tx_start)(dma_engine_t);
	a_status_t	(*rx_init)(dma_engine_t, dma_iftype_t);
	void		(*rx_config)(dma_engine_t, a_uint16_t, a_uint16_t);
	void		(*rx_start)(dma_engine_t);
	A_UINT32	(*intr_status)(dma_iftype_t);
	a_status_t	(*hard_xmit)(dma_engine_t, VBUF *);
	void		(*flush_xmit)(dma_engine_t);
	a_status_t	(*xmit_done)(dma_engine_t);
	VBUF		*(*reap_xmitted)(dma_engine_t);
	VBUF		*(*reap_recv)(dma_engine_t);
	void		(*return_recv)(dma_engine_t, VBUF *);
	a_status_t	(*recv_pkt)(dma_engine_t);
};


/**
 * @brief Install the DMA lib api's this for ROM patching
 *        support
 *
 * @param apis
 */
void dma_lib_module_install(struct dma_lib_api *);

#endif
