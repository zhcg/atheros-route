/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <adf_os_types.h>
#include <adf_nbuf.h>


/*
 * @brief This allocates an nbuf aligns if needed and reserves
 *        some space in the front, since the reserve is done
 *        after alignment the reserve value if being unaligned
 *        will result in an unaligned address.
 * 
 * @param hdl
 * @param size
 * @param reserve
 * @param align
 * 
 * @return nbuf or NULL if no memory
 */
struct sk_buff *
__adf_nbuf_alloc(adf_os_device_t osdev, size_t size, int reserve, int align)
{
    struct sk_buff *skb;
    unsigned long offset;

    if(align)
        size += (align - 1);

    skb = dev_alloc_skb(size);

    if (!skb) {
        printk("ERROR:NBUF alloc failed\n");
        return NULL;
    }

    /**
     * XXX:how about we reserve first then align
     */

    /**
     * Align & make sure that the tail & data are adjusted properly
     */
    if(align){
        offset = ((unsigned long) skb->data) % align;
        if(offset)
            skb_reserve(skb, align - offset);
    }

    /**
     * NOTE:alloc doesn't take responsibility if reserve unaligns the data
     * pointer
     */
    skb_reserve(skb, reserve);

    return skb;
}

/*
 * @brief free the nbuf its interrupt safe
 * @param skb
 */
void
__adf_nbuf_free(struct sk_buff *skb)
{
    dev_kfree_skb_any(skb);
}
/**
 * @brief create a nbuf map
 * @param osdev
 * @param dmap
 * 
 * @return a_status_t
 */
a_status_t
__adf_nbuf_dmamap_create(adf_os_device_t osdev, __adf_os_dma_map_t *dmap)
{
    a_status_t error = A_STATUS_OK;
    /**
     * XXX: driver can tell its SG capablity, it must be handled.
     * XXX: Bounce buffers if they are there
     */
    (*dmap) = kzalloc(sizeof(struct __adf_os_dma_map), GFP_KERNEL);
    if(!(*dmap))
        error = A_STATUS_ENOMEM;
    
    return error;
}

/**
 * @brief free the nbuf map
 * 
 * @param osdev
 * @param dmap
 */
void
__adf_nbuf_dmamap_destroy(adf_os_device_t osdev, __adf_os_dma_map_t dmap)
{
    kfree(dmap);
}

/**
 * @brief get the dma map of the nbuf
 * 
 * @param osdev
 * @param bmap
 * @param skb
 * @param dir
 * 
 * @return a_status_t
 */
a_status_t
__adf_nbuf_map(adf_os_device_t osdev, __adf_os_dma_map_t bmap, 
               struct sk_buff *skb, adf_os_dma_dir_t dir)
{
    int len0 = 0;
    struct skb_shared_info  *sh = skb_shinfo(skb);

    adf_os_assert((dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE));

    /**
     * if len != 0 then it's Tx or else Rx (data = tail)
     */
    len0                = (skb->len ? skb->len : skb_tailroom(skb));
    bmap->seg[0].daddr  = dma_map_single(osdev->dev, skb->data, len0, dir);
    bmap->seg[0].len    = len0;
    bmap->nsegs         = 1;
    bmap->mapped        = 1;

#ifndef __ADF_SUPPORT_FRAG_MEM
    adf_os_assert(sh->nr_frags == 0);
#else
    adf_os_assert(sh->nr_frags <= __ADF_OS_MAX_SCATTER);

    for (int i = 1; i <= sh->nr_frags; i++) {
        skb_frag_t *f           = &sh->frags[i-1]
        bmap->seg[i].daddr    = dma_map_page(osdev->dev, 
                                             f->page, 
                                             f->page_offset,
                                             f->size, 
                                             dir);
        bmap->seg[i].len      = f->size;
    }
    /**
     * XXX: IP fragments, generally linux api's recurse, but how
     * will our map look like
     */
    bmap->nsegs += i;
    adf_os_assert(sh->frag_list == NULL);
#endif
    return A_STATUS_OK;
}

/**
 * @brief adf_nbuf_unmap() - to unmap a previously mapped buf
 */
void
__adf_nbuf_unmap(adf_os_device_t osdev, __adf_os_dma_map_t bmap, 
                 adf_os_dma_dir_t dir)
{

    adf_os_assert(((dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE)));

    dma_unmap_single(osdev->dev, bmap->seg[0].daddr, bmap->seg[0].len, dir);

    bmap->mapped = 0;

    adf_os_assert(bmap->nsegs == 1);
    
}
/**
 * @brief return the dma map info 
 * 
 * @param[in]  bmap
 * @param[out] sg (map_info ptr)
 */
void 
__adf_nbuf_dmamap_info(__adf_os_dma_map_t bmap, adf_os_dmamap_info_t *sg)
{
    adf_os_assert(bmap->mapped);
    adf_os_assert(bmap->nsegs <= ADF_OS_MAX_SCATTER);
    
    memcpy(sg->dma_segs, bmap->seg, bmap->nsegs * 
           sizeof(struct __adf_os_segment));
    sg->nsegs = bmap->nsegs;
}
/**
 * @brief return the frag data & len, where frag no. is
 *        specified by the index
 * 
 * @param[in] buf
 * @param[out] sg (scatter/gather list of all the frags)
 * 
 */
void  
__adf_nbuf_frag_info(struct sk_buff *skb, adf_os_sglist_t  *sg)
{
    struct skb_shared_info  *sh = skb_shinfo(skb);

    adf_os_assert(skb != NULL);
    sg->sg_segs[0].vaddr = skb->data;
    sg->sg_segs[0].len   = skb->len;
    sg->nsegs            = 1;

#ifndef __ADF_SUPPORT_FRAG_MEM
    adf_os_assert(sh->nr_frags == 0);
#else
    for(int i = 1; i <= sh->nr_frags; i++){
        skb_frag_t    *f        = &sh->frags[i - 1];
        sg->sg_segs[i].vaddr    = (uint8_t *)(page_address(f->page) + 
                                  f->page_offset);
        sg->sg_segs[i].len      = f->size;

        adf_os_assert(i < ADF_OS_MAX_SGLIST);
    }
    sg->nsegs += i;
#endif
}

a_status_t 
__adf_nbuf_set_rx_cksum(struct sk_buff *skb, adf_nbuf_rx_cksum_t *cksum)
{
    switch (cksum->result) {
    case ADF_NBUF_RX_CKSUM_NONE:
        skb->ip_summed = CHECKSUM_NONE;
        break;
    case ADF_NBUF_RX_CKSUM_UNNECESSARY:
        skb->ip_summed = CHECKSUM_UNNECESSARY;
        break;
    case ADF_NBUF_RX_CKSUM_HW:
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,19)
        skb->ip_summed = CHECKSUM_HW;
#else
        skb->ip_summed = CHECKSUM_PARTIAL;
#endif
        skb->csum      = cksum->val;
        break;
    default:
        printk("ADF_NET:Unknown checksum type\n");
        adf_os_assert(0);
	return A_STATUS_ENOTSUPP;
    }
    return A_STATUS_OK;
}
a_status_t      
__adf_nbuf_get_vlan_info(adf_net_handle_t hdl, struct sk_buff *skb, 
                         adf_net_vlanhdr_t *vlan)
{
     return A_STATUS_OK;
}

void
__adf_nbuf_dmamap_set_cb(__adf_os_dma_map_t dmap, void *cb, void *arg)
{
    return;
}

EXPORT_SYMBOL(__adf_nbuf_alloc);
EXPORT_SYMBOL(__adf_nbuf_free);
EXPORT_SYMBOL(__adf_nbuf_frag_info);
EXPORT_SYMBOL(__adf_nbuf_dmamap_create);
EXPORT_SYMBOL(__adf_nbuf_dmamap_destroy);
EXPORT_SYMBOL(__adf_nbuf_map);
EXPORT_SYMBOL(__adf_nbuf_unmap);
EXPORT_SYMBOL(__adf_nbuf_dmamap_info);
EXPORT_SYMBOL(__adf_nbuf_set_rx_cksum);
EXPORT_SYMBOL(__adf_nbuf_get_vlan_info);
EXPORT_SYMBOL(__adf_nbuf_dmamap_set_cb);
