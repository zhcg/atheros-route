//#define pci_dma_test
#include <wasp_api.h>
#include <hif_api.h>
#include <dma_lib.h>
#include <hif_pci.h>
#include <vbuf_api.h>

#include "fwd.h"

fwd_tgt_softc_t fwd_sc;

void    fwd_tgt_recv(VBUF *hdr_buf, VBUF *buf, void *ctx );
void    fwd_retbuf_handler(VBUF *buf, void *ServiceCtx);

hif_handle_t
fwd_init()
{
	HIF_CALLBACK		hifconfig;
	A_UINT32		size, res_headroom;

	hifconfig.send_buf_done	= fwd_retbuf_handler;
	hifconfig.recv_buf	= fwd_tgt_recv;
	hifconfig.context	= &fwd_sc;

	res_headroom = HIF_get_reserved_headroom(NULL);

	size = sizeof(fwd_rsp_t) + res_headroom;

	HIF_register_callback(NULL, &hifconfig);

	HIF_get_default_pipe(NULL, &fwd_sc.rx_pipe, &fwd_sc.tx_pipe);

	return NULL;
}

void
fwd_retbuf_handler(VBUF *buf, void *ServiceCtx)
{
	// __pci_return_recv
	HIF_return_recv_buf(fwd_sc.hif_handle, fwd_sc.rx_pipe, buf);
}

#ifdef pci_dma_test
A_UINT32
fwd_tgt_process_last(volatile A_UINT32 *image, A_UINT32 size)
{
    int i, checksum   = 0;

    for (i = 0 ; i < size; i += 4, image++)
        checksum  =   checksum ^ *image;

    return checksum;
}
#else
a_status_t
fwd_tgt_process_last(A_UINT32 size, A_UINT32 cksum)
{
    int i, checksum   = 0;
    A_UINT32   *image = (A_UINT32 *)fwd_sc.addr;

    A_PRINTF("checksumming %p %u bytes: expected checksum: 0x%08x\n", image, size, cksum);

    for (i = 0 ; i < size; i += 4, image++)
        checksum  =   checksum ^ *image;

    A_PRINTF("\t\tchecksum = 0x%x\n", checksum);


    if (checksum == cksum)
        return A_STATUS_OK;
    else
        return A_STATUS_FAILED;
}
#endif

volatile A_UINT32 * bp_fn(volatile A_UINT32 *d)
{
	int		i = 0;
#if 0
	for (i = 0; i < (A_UINT32)d; i++) {
		i = i + 100;
		i = i/23;
	}
#endif
	d = (A_UINT32 *)i;
	return d;
}


void
fwd_tgt_recv(VBUF *hdr_buf, VBUF *buf, void *ctx )
{
    volatile a_uint8_t *data;
    A_UINT32 len, seglen, offset, i, more, eloc = 0;
    volatile A_UINT32 *image, *daddr;
    volatile fwd_cmd_t *c;
    volatile fwd_rsp_t *r;
    a_status_t  status = A_STATUS_FAILED;
    VDESC *desc = NULL;
#ifdef pci_dma_test
    static A_UINT32 pktno;
#endif

    data = buf->desc_list->buf_addr + buf->desc_list->data_offset;
    seglen  = buf->desc_list->data_size;

    c      =  (fwd_cmd_t *)data;
    len    =  c->len;
    offset =  c->offset;
    more   =  c->more_data;
    image  =  (A_UINT32 *)(c + 1);

    if (offset == 0) {
        fwd_sc.addr  = (A_UINT32)(*image);
        image ++;
    }

    daddr = (A_UINT32 *)(fwd_sc.addr + offset);
    //A_PRINTF("daddr = %p\n", daddr);

    if (!more) {
        len -= 4;
    }

    //if ((A_UINT32)daddr == 0x80472000) {
    //    bp_fn(daddr);
    //}
    //A_PRINTF("Copying to 0x%x\n", daddr);
	//if (((A_UINT32)daddr & 0xbd000000) == 0xbd000000) A_PRINTF("%p = %p %u\n", daddr, image, len);

	//if (((A_UINT32)daddr & 0xbd000000) != 0xbd000000)  A_PRINTF("%p = %p %u\n", daddr, image, len);
    for (i = 0 ; i < len; i += 4) {
        *daddr       =   *image;
	//if (((A_UINT32)daddr & 0xbd000000) != 0xbd000000) A_PRINTF("%p = %p\n", daddr, image);
	//A_PRINTF("\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r");
	//A_PRINTF("\r\r\r\r\r\r\r\r\r\r\r\r\r\r");
        image ++;
        daddr ++;
    }

    desc = buf->desc_list;
    while(desc->next_desc != NULL)
        desc = desc->next_desc;
    desc->data_size -= seglen;
    buf->buf_length -= seglen;

    r = (fwd_rsp_t *)(desc->buf_addr + desc->data_offset + desc->data_size);
    desc->data_size += sizeof(fwd_rsp_t);
    buf->buf_length += sizeof(fwd_rsp_t);

#ifndef pci_dma_test
    r->offset = c->offset;
#endif

    if (more) {
        r->rsp = FWD_RSP_ACK;
	status = A_STATUS_FAILED;
#ifdef pci_dma_test
	pktno ++;
        r->rx_pkt_cksum = fwd_tgt_process_last(daddr - (len / sizeof(A_UINT32)), len);
	A_PRINTF("%p[%u] cksum = 0x%x\n", daddr - (len / sizeof(A_UINT32)), pktno, r->offset);
#endif
        goto done;
    }

#ifndef pci_dma_test
    status = fwd_tgt_process_last(offset + len,  *image);

    /* reach to the jump location */
    image++;
    eloc   =  *image;

    if (status == A_STATUS_OK)
        r->rsp = FWD_RSP_SUCCESS;
    else
        r->rsp = FWD_RSP_FAILED;
#endif

done:
    // testing hack
    // r->offset = 0xbadc0ffe;
    // A_PRINTF("\t\t%p: r->rsp %u	r->offset %p\n", r, r->rsp, r->offset);

    HIF_send_buffer(fwd_sc.hif_handle, fwd_sc.tx_pipe, buf);	// __pci_xmit_buf

    if (!more && (status == A_STATUS_OK)) {
	A_PRINTF("Download complete. Jumping to %p\n", eloc);
        call_fw(HIF_PCIE, eloc);
    }
}
