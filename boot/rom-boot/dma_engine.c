/*
 * @File: dma_engine.c
 *
 * @Abstract: DMA engine for Magpie
 *
 * @Notes:
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All rights reserved.
 *
 */
//#include "sys_cfg.h"
#include "dt_defs.h"
#include "reg_defs.h"
#include "desc.h"
#include <wasp_api.h>

//#include <osapi.h>
//#include <HIF_api.h>
#include <dma_engine_api.h>
#include <vdesc_api.h>
#include <adf_os_mem.h>
//#include <adf_os_io.h>
#include <asm/addrspace.h>

//#include "HIF_usb.h"

//HIF_USB_CONTEXT g_hifUSBCtx;

#define VDESC_TO_USBDESC(vdesc)     (struct zsDmaDesc *)((vdesc)->hw_desc_buf)

static void relinkUSBDescToVdesc(VBUF *buf, struct zsDmaDesc* desc);
static void config_queue(struct zsDmaQueue *q, VDESC *desc_list);

#if ENABLE_SW_SWAP_DATA_MODE
static void swapData(struct zsDmaDesc* usbDesc);
#endif

static void init_usb_desc(struct zsDmaDesc *usbDesc)
{
    usbDesc->status = ZM_OWN_BITS_SW;
    usbDesc->ctrl = 0;
    usbDesc->dataSize = 0;
    usbDesc->totalLen = 0;
    usbDesc->lastAddr = 0;
    usbDesc->dataAddr = 0;
    usbDesc->nextAddr = 0;
}

void _DMAengine_init()
{

}

void _DMAengine_init_rx_queue(struct zsDmaQueue *q)
{
    VDESC *desc;
    struct zsDmaDesc *usbDesc;

    desc = VDESC_alloc_vdesc();
    if ( desc != NULL ) {
        usbDesc = VDESC_TO_USBDESC(desc);
        init_usb_desc(usbDesc);


        q->head = q->terminator = usbDesc;
    }
}

void _DMAengine_init_tx_queue(struct zsTxDmaQueue *q)
{
    _DMAengine_init_rx_queue((struct zsDmaQueue *)q);
    q->xmited_buf_head = NULL;
    q->xmited_buf_tail = NULL;
}

#if ENABLE_SW_SWAP_DATA_MODE

static void swapData(struct zsDmaDesc* usbDesc)
{
    int len = (usbDesc->dataSize & 0xfffffffc) >> 2;
    int i;
    A_UINT32 *dataAddr = (A_UINT32 *)get_data_addr(usbDesc);
    A_UINT32 data;

    if ( ( usbDesc->dataSize & 3 ) != 0 ) {
        len += 1;
    }

    for ( i = 0; i < len; i++ ) {
        data = dataAddr[i];

        dataAddr[i] = __bswap32(data);
    }
}

#endif

void _DMAengine_return_recv_buf(struct zsDmaQueue *q, VBUF *buf)
{
    /* Re-link the VDESC of buf into USB descriptor list & queue the descriptors
       into downQ
     */
    config_queue(q, buf->desc_list);
    VBUF_free_vbuf(buf);
}

static void config_queue(struct zsDmaQueue *q, VDESC *desc_list)
{
    VDESC *theDesc;
    struct zsDmaDesc *usbDesc;
    struct zsDmaDesc* prevUsbDesc = NULL;
    struct zsDmaDesc* headUsbDesc = NULL;

    theDesc = desc_list;
    while ( theDesc != NULL ) {
        usbDesc = (struct zsDmaDesc *)VDESC_get_hw_desc(theDesc);
        init_usb_desc(usbDesc);

        theDesc->data_offset = 0; //RAY 0723
        set_data_addr(usbDesc, (volatile u32_t)(theDesc->buf_addr + theDesc->data_offset));
        usbDesc->dataSize = theDesc->buf_size;

        if ( prevUsbDesc == NULL ) {
            headUsbDesc = usbDesc;
            prevUsbDesc = usbDesc;
        } else {
            set_next_addr(prevUsbDesc, usbDesc);
            prevUsbDesc = usbDesc;
        }
#if 0
	A_PRINTF("Rx: vdesc[%p] %p %p %u %u %u 0x%x\n",
		theDesc, theDesc->next_desc, theDesc->buf_addr,
		theDesc->buf_size, theDesc->data_offset, theDesc->data_size,
		theDesc->control);
	A_PRINTF("\tRx: pci[%p] 0x%x 0x%x 0x%x 0x%x %p %p %p\n",
		usbDesc, usbDesc->ctrl, usbDesc->status,
		usbDesc->totalLen, usbDesc->dataSize, usbDesc->lastAddr,
		usbDesc->dataAddr, usbDesc->nextAddr);
#endif

        theDesc = theDesc->next_desc;
    }

    set_last_addr(headUsbDesc, prevUsbDesc);
    DMA_Engine_reclaim_packet(q, headUsbDesc);

    return;
}

//#define MAX_TX_BUF_SIZE            ZM_BLOCK_SIZE
//#define MAX_TX_BUF_SIZE            1600

void _DMAengine_config_rx_queue(struct zsDmaQueue *q, int num_desc, int buf_size)
{
	//void _DMAengine_desc_dump(struct zsDmaQueue *q);
    int i;
	unsigned *p;
    VDESC *desc;
    VDESC *head = NULL;
#if 0
    struct zsDmaDesc *usbDesc;
#endif

    for(i=0; i < num_desc; i++)
    {
        desc = VDESC_alloc_vdesc();
	p = (unsigned *)desc;

        if (desc == NULL) {
            A_PRINTF("%s(%d): desc == NULL\n", __func__, __LINE__);
            while (1) { }
        }

	//A_PRINTF("Init desc at %p\n", desc);
	//A_PRINTF("0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", p[0], p[1], p[2], p[3], p[4]);
	//A_PRINTF("0x%08x 0x%08x 0x%08x 0x%08x\n", p[5], p[6], p[7], p[8]);
	//A_MEMZERO(desc, sizeof(*desc));
        desc->buf_addr = (A_UINT8 *)adf_os_mem_alloc(buf_size);
        desc->buf_size = buf_size;
        desc->next_desc = NULL;
        desc->data_offset = 0;
        desc->data_size = 0;
        desc->control = 0;
	//A_PRINTF("Rx: %p: 0x%08x 0x%08x 0x%08x 0x%08x\n", p, p[0], p[1], p[2], p[3]);

        if ( head == NULL )
        {
            head = desc;
            q->head = VDESC_TO_USBDESC(desc);
        }
        else
        {
            desc->next_desc = head;
            head = desc;
        }

#if 0
	{
        struct zsDmaDesc *usbDesc = VDESC_TO_USBDESC(desc);
	A_PRINTF("Rx: vdesc[%p] %p %p %u %u %u 0x%x\n",
		desc, desc->next_desc, desc->buf_addr,
		desc->buf_size, desc->data_offset, desc->data_size,
		desc->control);
	A_PRINTF("\tRx: usb[%p] 0x%x 0x%x 0x%x 0x%x %p %p %p\n",
		usbDesc, usbDesc->ctrl, usbDesc->status,
		usbDesc->totalLen, usbDesc->dataSize, usbDesc->lastAddr,
		usbDesc->dataAddr, usbDesc->nextAddr);
	}
#endif
    }

    config_queue(q, head);
}

void _DMAengine_xmit_buf(struct zsTxDmaQueue *q, VBUF *buf)
{
    VDESC *currVdesc;
    struct zsDmaDesc* usbDesc = NULL;
    struct zsDmaDesc* prevUsbDesc = NULL;
    struct zsDmaDesc* headUsbDesc = NULL;

    /* Re-link the VDESC of buf into USB descriptor list & queue the descriptors
       into upQ
     */
    currVdesc = (VDESC *)buf->desc_list;
    while(currVdesc != NULL) {

        usbDesc = (struct zsDmaDesc *)currVdesc->hw_desc_buf;

        init_usb_desc(usbDesc);
        usbDesc->dataSize = currVdesc->data_size;
        set_data_addr(usbDesc, (volatile u32_t)(currVdesc->buf_addr + currVdesc->data_offset));
        usbDesc->ctrl = 0;
        usbDesc->status = 0;

#if ENABLE_SW_SWAP_DATA_MODE && ENABLE_SWAP_DATA_MODE == 0
        swapData(usbDesc);
#endif

        if ( prevUsbDesc == NULL ) {
            headUsbDesc = usbDesc;

            usbDesc->ctrl |= ZM_FS_BIT;

            // how to get the total len???
            usbDesc->totalLen = buf->buf_length;
            prevUsbDesc = usbDesc;
        }
        else {
            set_next_addr(prevUsbDesc, usbDesc);
            prevUsbDesc = usbDesc;
        }

#if 0
	A_PRINTF("%s: vdesc[%p] %p %p %u %u %u 0x%x\n",
		__func__, currVdesc, currVdesc->next_desc, currVdesc->buf_addr,
		currVdesc->buf_size, currVdesc->data_offset, currVdesc->data_size,
		currVdesc->control);
	A_PRINTF("\tpci[%p] 0x%x 0x%x 0x%x 0x%x %p %p %p\n",
		usbDesc, usbDesc->ctrl, usbDesc->status,
		usbDesc->totalLen, usbDesc->dataSize, usbDesc->lastAddr,
		usbDesc->dataAddr, usbDesc->nextAddr);
#endif
        currVdesc = currVdesc->next_desc;
    }

    usbDesc->ctrl |= ZM_LS_BIT;
    set_last_addr(headUsbDesc, usbDesc);

    if ( q->xmited_buf_head == NULL && q->xmited_buf_tail == NULL ) {
        q->xmited_buf_head = buf;
        q->xmited_buf_tail = buf;
        q->xmited_buf_head->next_buf = q->xmited_buf_tail;
    }
    else {
        q->xmited_buf_tail->next_buf = buf;
        q->xmited_buf_tail = buf;
    }

    DMA_Engine_put_packet((struct zsDmaQueue *)q, headUsbDesc);
}

void _DMAengine_flush_xmit(struct zsDmaQueue *q)
{
}

int _DMAengine_has_compl_packets(struct zsDmaQueue *q)
{
    int has_compl_pkts = 0;

    if ((q->head != q->terminator) &&
        ((q->head->status & ZM_OWN_BITS_MASK) != ZM_OWN_BITS_HW)) {
        has_compl_pkts = 1;
    }

    return has_compl_pkts;
}

VBUF* _DMAengine_reap_recv_buf(struct zsDmaQueue *q)
{
    struct zsDmaDesc* desc;
    VBUF *buf;
    //int i;
    //u8_t *tbuf = (u8_t *)desc->dataAddr;

    desc = DMA_Engine_get_packet(q);

    if(!desc)
       return NULL;

#if ENABLE_SW_SWAP_DATA_MODE && ENABLE_SWAP_DATA_MODE == 0
    swapData(desc);
#endif

    buf = VBUF_alloc_vbuf();
    if (buf == NULL) {
        A_PRINTF("%s(%d): buf == NULL\n", __func__, __LINE__);
        while (1) { }
    }


    relinkUSBDescToVdesc(buf, desc);
    return buf;
}

VBUF* _DMAengine_reap_xmited_buf(struct zsTxDmaQueue *q)
{
    struct zsDmaDesc* desc;
    VBUF *sentBuf;

    desc = DMA_Engine_get_packet((struct zsDmaQueue *)q);

    if(!desc)
       return NULL;

    // assert g_hifUSBCtx.upVbufQ.head is not null
    // assert g_hifUSBCtx.upVbufQ.tail is not null
    sentBuf = q->xmited_buf_head;
    if ( q->xmited_buf_head == q->xmited_buf_tail ) {
        q->xmited_buf_head = NULL;
        q->xmited_buf_tail = NULL;
    } else {
        q->xmited_buf_head = q->xmited_buf_head->next_buf;
    }

    sentBuf->next_buf = NULL;
    relinkUSBDescToVdesc(sentBuf, desc);
    return sentBuf;
}

void _DMAengine_desc_dump(struct zsDmaQueue *q)
{
    u32_t i=0;
    struct zsDmaDesc* tmpDesc;

    tmpDesc = q->head;

    do {
        if( tmpDesc == q->terminator )
        {
#ifdef DESC_DUMP_BOTH_DESCnDATA
            A_PRINTF("0x%08x(0x%08x,T)]", tmpDesc, tmpDesc->dataAddr);
#else
            A_PRINTF("0x%08x(T)]", tmpDesc);
#endif
            break;
        }
        else
#ifdef DESC_DUMP_BOTH_DESCnDATA
            A_PRINTF("0x%08x(0x%08x,%c)->", tmpDesc, tmpDesc->dataAddr, (tmpDesc->status&ZM_OWN_BITS_HW)?'H':'S');
#else
            A_PRINTF("0x%08x(%c)->", tmpDesc, (tmpDesc->status&ZM_OWN_BITS_HW)?'H':'S');
#	if 0
		A_PRINTF("ctrl 0x%x, status 0x%x len 0x%x size 0x%x lastaddr %p data %p next %p\n",
			tmpDesc->ctrl,
			tmpDesc->status,
			tmpDesc->totalLen,
			tmpDesc->dataSize,
			tmpDesc->lastAddr,
			tmpDesc->dataAddr,
			tmpDesc->nextAddr);
#	endif
#endif

        if( (++i%5)==0 )
        {
            A_PRINTF("\n\r   ");
        }

        tmpDesc = get_next_addr(tmpDesc);
    }while(1);
    A_PRINTF("\n\r");
}

/* the exported entry point into this module. All apis are accessed through
 * function pointers */
void dma_engine_module_install(struct dma_engine_api *apis)
{
        /* hook in APIs */
    apis->_init                 = _DMAengine_init;
    apis->_config_rx_queue      = _DMAengine_config_rx_queue;
    apis->_xmit_buf             = _DMAengine_xmit_buf;
    apis->_flush_xmit           = _DMAengine_flush_xmit;
    apis->_reap_recv_buf        = _DMAengine_reap_recv_buf;
    apis->_return_recv_buf      = _DMAengine_return_recv_buf;
    apis->_reap_xmited_buf      = _DMAengine_reap_xmited_buf;
#if ENABLE_SW_SWAP_DATA_MODE && ENABLE_SWAP_DATA_MODE == 0
    apis->_swap_data            = swapData;
#endif
    apis->_has_compl_packets    = _DMAengine_has_compl_packets;
    apis->_init_rx_queue        = _DMAengine_init_rx_queue;
    apis->_init_tx_queue        = _DMAengine_init_tx_queue;
    apis->_desc_dump            = _DMAengine_desc_dump;
    apis->_get_packet           = zfDmaGetPacket;
    apis->_reclaim_packet       = zfDmaReclaimPacket;
    apis->_put_packet           = zfDmaPutPacket;

        /* save ptr to the ptr to the context for external code to inspect/modify internal module state */
    //apis->pReserved = &g_pMboxHWContext;
}

static void relinkUSBDescToVdesc(VBUF *buf, struct zsDmaDesc* desc)
{
    VDESC *vdesc;
    VDESC *prevVdesc = NULL;
    struct zsDmaDesc *currDesc = desc;

    vdesc = VDESC_HW_TO_VDESC(currDesc);
    buf->desc_list = vdesc;
    buf->buf_length = currDesc->totalLen;

    while(currDesc != NULL) {
	//A_PRINTF("%s: curr %p %p vdesc %p prev %p desc %p\n",
	//	__func__, currDesc, currDesc->nextAddr, vdesc, prevVdesc, desc);
        vdesc->data_size = currDesc->dataSize;
        //vdesc->data_offset = 0; // TODO: bad!!

        if ( prevVdesc == NULL ) {
            prevVdesc = vdesc;
        } else {
            prevVdesc->next_desc = vdesc;
            prevVdesc = vdesc;
        }

        if ( currDesc->ctrl & ZM_LS_BIT ) {
            vdesc->next_desc = NULL;
            currDesc = NULL;
            break;
        } else {
            currDesc = get_next_addr(currDesc);
            vdesc = VDESC_HW_TO_VDESC(currDesc);
        }
    }
}
