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

/*
 * Linux implemenation of skbuf
 */
#ifndef _ADF_CMN_NET_PVT_BUF_H
#define _ADF_CMN_NET_PVT_BUF_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <asm/types.h>
#include <asm/scatterlist.h>
#include <adf_os_types.h>

#define __ADF_NBUF_NULL   NULL

/*
 * Use socket buffer as the underlying implentation as skbuf .
 * Linux use sk_buff to represent both packet and data,
 * so we use sk_buffer to represent both skbuf .
 */
typedef struct sk_buff *        __adf_nbuf_t;

#define OSDEP_EAPOL_TID 6  /* send it on VO queue */

/*
 * Transmitted frames have the following information
 * held in the sk_buff control buffer.  This is used to
 * communicate various inter-procedural state that needs
 * to be associated with the frame for the duration of
 * it's existence.
 *
 * NB: sizeof(cb) == 48 and the vlan code grabs the first
 *     8 bytes so we reserve/avoid it.
 */
struct nbuf_cb {
	u_int8_t		vlan[8];	/* reserve for vlan tag info */
	struct ieee80211_node	*ni;
	void			*context;	/* pointer to context area */
	u_int32_t		flags;
#define	N_LINK0		0x01			/* frame needs WEP encryption */
#define	N_FF		0x02			/* fast frame */
#define	N_PWR_SAV	0x04			/* bypass power save handling */
#define N_UAPSD		0x08			/* frame flagged for u-apsd handling */
#define N_EAPOL		0x10			/* frame flagged for EAPOL handling */
#define N_AMSDU		0x20			/* frame flagged for AMSDU handling */
#define	N_NULL_PWR_SAV	0x40			/* null data with power save bit on */
#define N_PROBING	0x80			/* frame flagged as a probing one */
#define N_ERROR         0x100                   /* frame flagged as a error one */
#define N_MOREDATA      0x200                   /* more data flag */
#define N_SMPSACTM      0x400                   /* This frame is SM power save Action Mgmt frame */
#define N_QOS           0x800                   /* This is a QOS frame*/
#define N_ENCAP_DONE    0x1000              /* This frame is marked as fast-path pkts, some encapsulation work has been done by h/w */
#define N_CLONED		0x2000		/* frame is cloned in rx path */
#ifdef ATH_SUPPORT_WAPI
#define N_WAI           0x4000                  /* frame flagged for WAPI handling */
#endif
	u_int8_t		u_tid;		/* user priority from vlan/ip tos   */
        u_int8_t                exemptiontype;  /* exemption type of this frame (0,1,2)*/
        u_int8_t                type;           /* type of this frame */
#if defined(ATH_SUPPORT_VOWEXT) || defined(ATH_SUPPORT_IQUE) || defined(VOW_LOGLATENCY) || UMAC_SUPPORT_NAWDS != 0
        u_int8_t		firstxmit;
        u_int32_t		firstxmitts;
#endif

#if defined (ATH_SUPPORT_P2P)
        void            *complete_handler;     /* complete handler */ 
        void            *complete_handler_arg; /* complete handler arg */
#endif
#if defined(VOW_TIDSCHED) || defined(ATH_SUPPORT_IQUE) || defined(VOW_LOGLATENCY)
	u_int32_t		qin_timestamp;           /* timestamp of buffer's entry into queue */
#endif
};

#ifndef ATH_SUPPORT_HTC
/*
 * For packet capture, define the same physical layer packet header 
 * structure as used in the wlan-ng driver 
 */
typedef struct {
	u_int32_t did;
	u_int16_t status;
	u_int16_t len;
	u_int32_t data;
} p80211item_uint32_t;

typedef struct {
	u_int32_t msgcode;
	u_int32_t msglen;
#define WLAN_DEVNAMELEN_MAX 16
	u_int8_t devname[WLAN_DEVNAMELEN_MAX];
	p80211item_uint32_t hosttime;
	p80211item_uint32_t mactime;
	p80211item_uint32_t channel;
	p80211item_uint32_t rssi;
	p80211item_uint32_t sq;
	p80211item_uint32_t signal;
	p80211item_uint32_t noise;
	p80211item_uint32_t rate;
	p80211item_uint32_t istx;
	p80211item_uint32_t frmlen;
} wlan_ng_prism2_header;

#define ieee80211_cb nbuf_cb
#endif

#define N_FLAG_SET(_nbf, _flag) \
        (((struct nbuf_cb *)(_nbf)->cb)->flags |= (_flag))
#define N_FLAG_CLR(_nbf, _flag) \
        (((struct nbuf_cb *)(_nbf)->cb)->flags &= ~(_flag))
#define N_FLAG_GET(_nbf, _flag) \
        (((struct nbuf_cb *)(_nbf)->cb)->flags & (_flag))
#define N_FLAG_IS(_nbf, _flag) \
        ((((struct nbuf_cb *)(_nbf)->cb)->flags & (_flag)) == (_flag))
#define N_FLAG_KEEP_ONLY(_nbf, _flag) \
        (((struct nbuf_cb *)(_nbf)->cb)->flags &= (_flag))

#define N_PWR_SAV_SET(nbf) N_FLAG_SET((nbf), N_PWR_SAV)
#define N_PWR_SAV_CLR(nbf) N_FLAG_CLR((nbf), N_PWR_SAV)
#define N_PWR_SAV_GET(nbf) N_FLAG_GET((nbf), N_PWR_SAV)
#define N_PWR_SAV_IS(nbf)  N_FLAG_IS((nbf), N_PWR_SAV)

#define N_NULL_PWR_SAV_SET(nbf) N_FLAG_SET((nbf), N_NULL_PWR_SAV)
#define N_NULL_PWR_SAV_CLR(nbf) N_FLAG_CLR((nbf), N_NULL_PWR_SAV)
#define N_NULL_PWR_SAV_GET(nbf) N_FLAG_GET((nbf), N_NULL_PWR_SAV)
#define N_NULL_PWR_SAV_IS(nbf)  N_FLAG_IS((nbf), N_NULL_PWR_SAV)

#define N_PROBING_SET(nbf) N_FLAG_SET((nbf), N_PROBING)
#define N_PROBING_CLR(nbf) N_FLAG_CLR((nbf), N_PROBING)
#define N_PROBING_GET(nbf) N_FLAG_GET((nbf), N_PROBING)
#define N_PROBING_IS(nbf)  N_FLAG_IS((nbf), N_PROBING)

#define N_CLONED_SET(nbf) N_FLAG_SET((nbf), N_CLONED)
#define N_CLONED_CLR(nbf) N_FLAG_CLR((nbf), N_CLONED)
#define N_CLONED_GET(nbf) N_FLAG_GET((nbf), N_CLONED)
#define N_CLONED_IS(nbf)  N_FLAG_IS((nbf), N_CLONED)

#define N_MOREDATA_SET(nbf) N_FLAG_SET((nbf), N_MOREDATA)
#define N_MOREDATA_CLR(nbf) N_FLAG_CLR((nbf), N_MOREDATA)
#define N_MOREDATA_GET(nbf) N_FLAG_GET((nbf), N_MOREDATA)
#define N_MOREDATA_IS(nbf)  N_FLAG_IS((nbf), N_MOREDATA)

#define N_SMPSACTM_SET(nbf) N_FLAG_SET((nbf), N_SMPSACTM)
#define N_SMPSACTM_CLR(nbf) N_FLAG_CLR((nbf), N_SMPSACTM)
#define N_SMPSACTM_GET(nbf) N_FLAG_GET((nbf), N_SMPSACTM)
#define N_SMPSACTM_IS(nbf)  N_FLAG_IS((nbf), N_SMPSACTM)

#define N_QOS_SET(nbf)      N_FLAG_SET((nbf), N_QOS)
#define N_QOS_CLR(nbf)      N_FLAG_CLR((nbf), N_QOS)
#define N_QOS_GET(nbf)      N_FLAG_GET((nbf), N_QOS)
#define N_QOS_IS(nbf)       N_FLAG_IS((nbf), N_QOS)

#define N_EAPOL_SET(nbf)    N_FLAG_SET((nbf), N_EAPOL)
#define N_EAPOL_IS(nbf)     N_FLAG_IS((nbf), N_EAPOL)

#ifdef ATH_SUPPORT_WAPI
#define N_WAI_SET(nbf)    N_FLAG_SET((nbf), N_WAI)
#define N_WAI_IS(nbf)     N_FLAG_IS((nbf), N_WAI)
#endif

#define N_AMSDU_SET(nbf)    N_FLAG_SET((nbf), N_AMSDU)
#define N_AMSDU_IS(nbf)     N_FLAG_IS((nbf), N_AMSDU)

#define N_FF_SET(nbf)       N_FLAG_SET((nbf), N_FF)
#define N_FF_IS(nbf)        N_FLAG_IS((nbf), N_FF)

#define N_UAPSD_SET(nbf)    N_FLAG_SET((nbf), N_UAPSD)
#define N_UAPSD_CLR(nbf)    N_FLAG_CLR((nbf), N_UAPSD)
#define N_UAPSD_IS(nbf)     N_FLAG_IS((nbf), N_UAPSD)

#define N_ENCAP_DONE_IS(nbf)    N_FLAG_IS((nbf), N_ENCAP_DONE)
#define N_ENCAP_DONE_SET(nbf)   N_FLAG_SET((nbf), N_ENCAP_DONE)
#define N_ENCAP_DONE_CLR(nbf)   N_FLAG_CLR((nbf), N_ENCAP_DONE)

#define N_STATUS_SET(nbf, _status)    \
        if(_status != WB_STATUS_OK)    \
            N_FLAG_SET((nbf), N_ERROR);
 
#define N_STATUS_GET(nbf)              \
        if(N_FLAG_IS((nbf), N_ERROR))  \
            return WB_STATUS_TX_ERROR; \
        else                           \
            return WB_STATUS_OK;

#define N_CONTEXT_SET(_nbf, _context)   \
        (((struct nbuf_cb *)(_nbf)->cb)->context = (_context))
#define N_CONTEXT_GET(_nbf)             \
        (((struct nbuf_cb *)(_nbf)->cb)->context)

#define N_TYPE_SET(_nbf, _type) \
        (((struct nbuf_cb *)(_nbf)->cb)->type = (_type))
#define N_TYPE_GET(_nbf) \
        (((struct nbuf_cb *)(_nbf)->cb)->type)

#define N_NODE_SET(_nbf, _ni) \
        (((struct nbuf_cb *)(_nbf)->cb)->ni = (_ni))
#define N_NODE_GET(_nbf) \
        (((struct nbuf_cb *)(_nbf)->cb)->ni)

#define N_EXMTYPE_SET(_nbf, _type) \
        (((struct nbuf_cb *)(_nbf)->cb)->exemptiontype = (_type))
#define N_EXMTYPE_GET(_nbf) \
        (((struct nbuf_cb *)(_nbf)->cb)->exemptiontype)

#define N_TID_SET(_nbf, _tid) \
        (((struct nbuf_cb *)(_nbf)->cb)->u_tid = (_tid))
#define N_TID_GET(_nbf) \
        (((struct nbuf_cb *)(_nbf)->cb)->u_tid)

/*
 * nbufs on the power save queue are tagged with an age and
 * timed out.  We reuse the hardware checksum field in the
 * nbuf packet header to store this data.
 * XXX use private cb area
 */
#define N_AGE_SET        __adf_nbuf_set_age
#define N_AGE_GET        __adf_nbuf_get_age
#define N_AGE_SUB        __adf_nbuf_adj_age

typedef struct __adf_nbuf_qhead {
    struct sk_buff   *head;
    struct sk_buff   *tail;
    unsigned int     qlen;
}__adf_nbuf_queue_t;

// typedef struct sk_buff_head     __adf_nbuf_queue_t;

// struct anet_dma_info {
//     dma_addr_t  daddr;
//     uint32_t    len;
// };
//
// struct __adf_nbuf_map {
//     int                   nelem;
//     struct anet_dma_info  dma[1];
// };
//
// typedef struct __adf_nbuf_map     *__adf_nbuf_dmamap_t;


/*
 * Use sk_buff_head as the implementation of adf_nbuf_queue_t.
 * Because the queue head will most likely put in some structure,
 * we don't use pointer type as the definition.
 */


/*
 * prototypes. Implemented in adf_nbuf_pvt.c
 */
__adf_nbuf_t    __adf_nbuf_alloc(__adf_os_device_t osdev, size_t size, int reserve, int align);
void            __adf_nbuf_free (struct sk_buff *skb);
a_status_t      __adf_nbuf_dmamap_create(__adf_os_device_t osdev, 
                                         __adf_os_dma_map_t *dmap);
void            __adf_nbuf_dmamap_destroy(__adf_os_device_t osdev, 
                                          __adf_os_dma_map_t dmap);
a_status_t      __adf_nbuf_map(__adf_os_device_t osdev, __adf_os_dma_map_t dmap,
                                struct sk_buff *skb, adf_os_dma_dir_t dir);
void            __adf_nbuf_unmap(__adf_os_device_t osdev, __adf_os_dma_map_t dmap,
                               adf_os_dma_dir_t dir);
void            __adf_nbuf_dmamap_info(__adf_os_dma_map_t bmap, adf_os_dmamap_info_t *sg);
void            __adf_nbuf_frag_info(struct sk_buff *skb, adf_os_sglist_t  *sg);
void            __adf_nbuf_dmamap_set_cb(__adf_os_dma_map_t dmap, void *cb, void *arg);


static inline a_status_t
__adf_os_to_status(signed int error)
{
    switch(error){
    case 0:
        return A_STATUS_OK;
    case ENOMEM:
    case -ENOMEM:
        return A_STATUS_ENOMEM;
    default:
        return A_STATUS_ENOTSUPP;
    }
}



/**
 * @brief This keeps the skb shell intact expands the headroom
 *        in the data region. In case of failure the skb is
 *        released.
 * 
 * @param skb
 * @param headroom
 * 
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_realloc_headroom(struct sk_buff *skb, uint32_t headroom) 
{
    if(pskb_expand_head(skb, headroom, 0, GFP_ATOMIC)){
        dev_kfree_skb_any(skb);
        skb = NULL;
    }
    return skb;
}
/**
 * @brief This keeps the skb shell intact exapnds the tailroom
 *        in data region. In case of failure it releases the
 *        skb.
 * 
 * @param skb
 * @param tailroom
 * 
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_realloc_tailroom(struct sk_buff *skb, uint32_t tailroom)
{
    if(likely(!pskb_expand_head(skb, 0, tailroom, GFP_ATOMIC)))
        return skb;
    /**
     * unlikely path
     */
    dev_kfree_skb_any(skb);
    return NULL;
}
/**
 * @brief return the amount of valid data in the skb, If there
 *        are frags then it returns total length.
 * 
 * @param skb
 * 
 * @return size_t
 */
static inline size_t
__adf_nbuf_len(struct sk_buff * skb)
{
    return skb->len;
}


/**
 * @brief link two nbufs, the new buf is piggybacked into the
 *        older one. The older (src) skb is released.
 *
 * @param dst( buffer to piggyback into)
 * @param src (buffer to put)
 *
 * @return a_status_t (status of the call) if failed the src skb
 *         is released
 */
static inline a_status_t
__adf_nbuf_cat(struct sk_buff *dst, struct sk_buff *src)
{
    a_status_t error = 0;

    adf_os_assert(dst && src);

    if(!(error = pskb_expand_head(dst, 0, src->len, GFP_ATOMIC)))
        memcpy(skb_put(dst, src->len), src->data, src->len);

    dev_kfree_skb_any(src);

    return __adf_os_to_status(error);
}
/**
 * @brief  create a version of the specified nbuf whose contents
 *         can be safely modified without affecting other
 *         users.If the nbuf is a clone then this function
 *         creates a new copy of the data. If the buffer is not
 *         a clone the original buffer is returned.
 *
 * @param skb (source nbuf to create a writable copy from)
 *
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_unshare(struct sk_buff *skb)
{
    return skb_unshare(skb, GFP_ATOMIC);
}
/**************************nbuf manipulation routines*****************/

static inline int
__adf_nbuf_headroom(struct sk_buff *skb)
{
    return skb_headroom(skb);
}
/**
 * @brief return the amount of tail space available
 *
 * @param buf
 *
 * @return amount of tail room
 */
static inline uint32_t
__adf_nbuf_tailroom(struct sk_buff *skb)
{
    return skb_tailroom(skb);
}

/**
 * @brief Push data in the front
 *
 * @param[in] buf      buf instance
 * @param[in] size     size to be pushed
 *
 * @return New data pointer of this buf after data has been pushed,
 *         or NULL if there is not enough room in this buf.
 */
static inline uint8_t *
__adf_nbuf_push_head(struct sk_buff *skb, size_t size)
{
    return skb_push(skb, size);
}
/**
 * @brief Puts data in the end
 *
 * @param[in] buf      buf instance
 * @param[in] size     size to be pushed
 *
 * @return data pointer of this buf where new data has to be
 *         put, or NULL if there is not enough room in this buf.
 */
static inline uint8_t *
__adf_nbuf_put_tail(struct sk_buff *skb, size_t size)
{
   if (skb_tailroom(skb) < size) {
       if(unlikely(pskb_expand_head(skb, 0, size-skb_tailroom(skb), GFP_ATOMIC))) {
           dev_kfree_skb_any(skb);
           return NULL;
       }
   }     
   return skb_put(skb, size);
}

/**
 * @brief pull data out from the front
 *
 * @param[in] buf   buf instance
 * @param[in] size  size to be popped
 *
 * @return New data pointer of this buf after data has been popped,
 *         or NULL if there is not sufficient data to pull.
 */
static inline uint8_t *
__adf_nbuf_pull_head(struct sk_buff *skb, size_t size)
{
    return skb_pull(skb, size);
}
/**
 *
 * @brief trim data out from the end
 *
 * @param[in] buf   buf instance
 * @param[in] size     size to be popped
 *
 * @return none
 */
static inline void
__adf_nbuf_trim_tail(struct sk_buff *skb, size_t size)
{
    return skb_trim(skb, skb->len - size);
}

/**
 * @brief test whether the nbuf is cloned or not
 *
 * @param buf
 *
 * @return a_bool_t (TRUE if it is cloned or else FALSE)
 */
static inline a_bool_t
__adf_nbuf_is_cloned(struct sk_buff *skb)
{
        return skb_cloned(skb);
}


/*********************nbuf private buffer routines*************/

/**
 * @brief get the priv pointer from the nbuf'f private space
 *
 * @param buf
 *
 * @return data pointer to typecast into your priv structure
 */
static inline uint8_t *
__adf_nbuf_get_priv(struct sk_buff *skb)
{
        return &skb->cb[8];
}

/**
 * @brief This will return the header's addr & m_len
 */
static inline void
__adf_nbuf_peek_header(struct sk_buff *skb, uint8_t   **addr, 
                       uint32_t    *len)
{
    *addr = skb->data;
    *len  = skb->len;
}

 /* adf_nbuf_queue_init() - initialize a skbuf queue
 */
/* static inline void */
/* __adf_nbuf_queue_init(struct sk_buff_head *qhead) */
/* { */
/*     skb_queue_head_init(qhead); */
/* } */

/* /\* */
/*  * adf_nbuf_queue_add() - add a skbuf to the end of the skbuf queue */
/*  * */
/*  * We use the non-locked version because */
/*  * there's no need to use the irq safe version of spinlock. */
/*  * However, the caller has to do synchronization by itself. */
/*  *\/ */
/* static inline void */
/* __adf_nbuf_queue_add(struct sk_buff_head *qhead,  */
/*                      struct sk_buff *skb) */
/* { */
/*     __skb_queue_tail(qhead, skb); */
/*     adf_os_assert(qhead->next == qhead->prev); */
/* } */

/* /\* */
/*  * adf_nbuf_queue_remove() - remove a skbuf from the head of the skbuf queue */
/*  * */
/*  * We use the non-locked version because */
/*  * there's no need to use the irq safe version of spinlock. */
/*  * However, the caller has to do synchronization by itself. */
/*  *\/ */
/* static inline struct sk_buff * */
/* __adf_nbuf_queue_remove(struct sk_buff_head * qhead) */
/* { */
/*     adf_os_assert(qhead->next == qhead->prev); */
/*     return __skb_dequeue(qhead); */
/* } */

/* static inline uint32_t */
/* __adf_nbuf_queue_len(struct sk_buff_head * qhead) */
/* { */
/*         adf_os_assert(qhead->next == qhead->prev); */
/*         return qhead->qlen; */
/* } */
/* /\** */
/*  * @brief returns the first guy in the Q */
/*  * @param qhead */
/*  *  */
/*  * @return (NULL if the Q is empty) */
/*  *\/ */
/* static inline struct sk_buff *    */
/* __adf_nbuf_queue_first(struct sk_buff_head *qhead) */
/* { */
/*    adf_os_assert(qhead->next == qhead->prev); */
/*    return (skb_queue_empty(qhead) ? NULL : qhead->next); */
/* } */
/* /\** */
/*  * @brief return the next packet from packet chain */
/*  *  */
/*  * @param buf (packet) */
/*  *  */
/*  * @return (NULL if no packets are there) */
/*  *\/ */
/* static inline struct sk_buff *    */
/* __adf_nbuf_queue_next(struct sk_buff *skb) */
/* { */
/*   return NULL; // skb->next; */
/* } */
/* /\** */
/*  * adf_nbuf_queue_empty() - check if the skbuf queue is empty */
/*  *\/ */
/* static inline a_bool_t */
/* __adf_nbuf_is_queue_empty(struct sk_buff_head *qhead) */
/* { */
/*     adf_os_assert(qhead->next == qhead->prev); */
/*     return skb_queue_empty(qhead); */
/* } */

/******************Custom queue*************/



/**
 * @brief initiallize the queue head
 * 
 * @param qhead
 */
static inline a_status_t
__adf_nbuf_queue_init(__adf_nbuf_queue_t *qhead)
{
    memset(qhead, 0, sizeof(struct __adf_nbuf_qhead));
    return A_STATUS_OK;
}


/**
 * @brief add an skb in the tail of the queue. This is a
 *        lockless version, driver must acquire locks if it
 *        needs to synchronize
 * 
 * @param qhead
 * @param skb
 */
static inline void
__adf_nbuf_queue_add(__adf_nbuf_queue_t *qhead, 
                     struct sk_buff *skb)
{
    skb->next = NULL;/*Nullify the next ptr*/

    if(!qhead->head)
        qhead->head = skb;
    else
        qhead->tail->next = skb;
    
    qhead->tail = skb;
    qhead->qlen++;
}

/**
 * @brief remove a skb from the head of the queue, this is a
 *        lockless version. Driver should take care of the locks
 * 
 * @param qhead
 * 
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_queue_remove(__adf_nbuf_queue_t * qhead)
{
    struct sk_buff  *tmp;

    if((tmp = qhead->head) == NULL)     
        return NULL;/*Q was empty already*/
    
    qhead->head = tmp->next;/*XXX:tail still points to tmp*/
    qhead->qlen--;
    tmp->next   = NULL;/*remove any references to the Q for the tmp*/
    
    return tmp;
}
/**
 * @brief return the queue length
 * 
 * @param qhead
 * 
 * @return uint32_t
 */
static inline uint32_t
__adf_nbuf_queue_len(__adf_nbuf_queue_t * qhead)
{
        return qhead->qlen;
}
/**
 * @brief returns the first skb in the queue
 * 
 * @param qhead
 * 
 * @return (NULL if the Q is empty)
 */
static inline struct sk_buff *   
__adf_nbuf_queue_first(__adf_nbuf_queue_t *qhead)
{
    return qhead->head;
}
/**
 * @brief return the next skb from packet chain, remember the
 *        skb is still in the queue
 * 
 * @param buf (packet)
 * 
 * @return (NULL if no packets are there)
 */
static inline struct sk_buff *   
__adf_nbuf_queue_next(struct sk_buff *skb)
{
    return skb->next;
}
/**
 * @brief check if the queue is empty or not
 * 
 * @param qhead
 * 
 * @return a_bool_t
 */
static inline a_bool_t
__adf_nbuf_is_queue_empty(__adf_nbuf_queue_t *qhead)
{
    return (qhead->qlen == 0);
}

/*
 * Use sk_buff_head as the implementation of adf_nbuf_queue_t.
 * Because the queue head will most likely put in some structure,
 * we don't use pointer type as the definition.
 */


/*
 * prototypes. Implemented in adf_nbuf_pvt.c
 */
a_status_t          __adf_nbuf_set_rx_cksum(struct sk_buff *skb, 
                                        adf_nbuf_rx_cksum_t *cksum);
a_status_t      __adf_nbuf_get_vlan_info(adf_net_handle_t hdl, 
                                         struct sk_buff *skb, 
                                         adf_net_vlanhdr_t *vlan);
/*
 * adf_nbuf_pool_init() implementation - do nothing in Linux
 */
static inline a_status_t
__adf_nbuf_pool_init(adf_net_handle_t anet)
{
    return A_STATUS_OK;
}

/*
 * adf_nbuf_pool_delete() implementation - do nothing in linux
 */
#define __adf_nbuf_pool_delete(osdev)


/**
 * @brief Expand both tailroom & headroom. In case of failure
 *        release the skb.
 * 
 * @param skb
 * @param headroom
 * @param tailroom
 * 
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_expand(struct sk_buff *skb, uint32_t headroom, uint32_t tailroom)
{
    if(likely(!pskb_expand_head(skb, headroom, tailroom, GFP_ATOMIC)))
        return skb;

    dev_kfree_skb_any(skb);
    return NULL;
}


/**
 * @brief clone the nbuf (copy is readonly)
 *
 * @param src_nbuf (nbuf to clone from)
 * @param dst_nbuf (address of the cloned nbuf)
 *
 * @return status
 * 
 * @note if GFP_ATOMIC is overkill then we can check whether its
 *       called from interrupt context and then do it or else in
 *       normal case use GFP_KERNEL
 * @example     use "in_irq() || irqs_disabled()"
 * 
 * 
 */
static inline struct sk_buff *
__adf_nbuf_clone(struct sk_buff *skb)
{   
    return skb_clone(skb, GFP_ATOMIC);
}
/**
 * @brief returns a private copy of the skb, the skb returned is
 *        completely modifiable
 * 
 * @param skb
 * 
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_copy(struct sk_buff *skb)
{
    return skb_copy(skb, GFP_ATOMIC);
}

#define __adf_nbuf_reserve      skb_reserve

/***********************XXX: misc api's************************/
static inline a_bool_t
__adf_nbuf_tx_cksum_info(struct sk_buff *skb, uint8_t **hdr_off, 
                         uint8_t **where)
{
//     if (skb->ip_summed == CHECKSUM_NONE)
//         return A_FALSE;
//
//     if (skb->ip_summed == CHECKSUM_HW) {
//         *hdr_off = (uint8_t *)(skb->h.raw - skb->data);
//         *where   = *hdr_off + skb->csum;
//         return A_TRUE;
//     }

    adf_os_assert(0);
    return A_FALSE;
}


/*
 * XXX What about other unions in skb? Windows might not have it, but we
 * should penalize linux drivers for it.
 * Besides this function is not likely doint the correct thing.
 */
static inline a_status_t
__adf_nbuf_get_tso_info(struct sk_buff *skb, adf_nbuf_tso_t *tso)
{
    adf_os_assert(0);
    return A_STATUS_ENOTSUPP;
/*
    if (!skb_shinfo(skb)->tso_size) {
        tso->type = adf_net_TSO_NONE;
        return;
    }

    tso->mss      = skb_shinfo(skb)->tso_size;
*/  
//     tso->hdr_off  = (uint8_t)(skb->h.raw - skb->data);
//
//     if (skb->protocol == ntohs(ETH_P_IP))
//         tso->type =  ADF_NET_TSO_IPV4;
//     else if (skb->protocol == ntohs(ETH_P_IPV6))
//         tso->type =  ADF_NET_TSO_ALL;
//     else
//         tso->type = ADF_NET_TSO_NONE;
}

/**
 * @brief return the pointer to data header in the skb
 * 
 * @param skb
 * 
 * @return uint8_t *
 */
static inline uint8_t *
__adf_nbuf_data(struct sk_buff * skb)
{
    return skb->data;
}

/**
 * @brief return the priority value of the skb
 * 
 * @param skb
 * 
 * @return uint32_t
 */
static inline uint32_t
__adf_nbuf_get_priority(struct sk_buff * skb)
{
    return skb->priority;
}

/**
 * @brief sets the priority value of the skb
 * 
 * @param skb, priority
 * 
 * @return void
 */
static inline void
__adf_nbuf_set_priority(struct sk_buff * skb, uint32_t p)
{
    skb->priority = p;
}

/**
 * @brief sets the next skb pointer of the current skb
 * 
 * @param skb and next_skb
 * 
 * @return void
 */
static inline void
__adf_nbuf_set_next(struct sk_buff * skb, struct sk_buff * skb_next)
{
    skb->next = skb_next;
}

/**
 * @brief return the checksum value of the skb
 * 
 * @param skb
 * 
 * @return uint32_t
 */
static inline uint32_t
__adf_nbuf_get_age(struct sk_buff * skb)
{
    return skb->csum;
}

/**
 * @brief sets the checksum value of the skb
 * 
 * @param skb, value
 * 
 * @return void
 */
static inline void
__adf_nbuf_set_age(struct sk_buff * skb, uint32_t v)
{
    skb->csum = v;
}

/**
 * @brief adjusts the checksum/age value of the skb
 * 
 * @param skb, adj
 * 
 * @return void
 */
static inline void
__adf_nbuf_adj_age(struct sk_buff * skb, uint32_t adj)
{
    skb->csum -= adj;
}

/**
 * @brief return the length of the copy bits for skb
 * 
 * @param skb, offset, len, to
 * 
 * @return int32_t
 */
static inline int32_t
__adf_nbuf_copy_bits(struct sk_buff * skb, int32_t offset, int32_t len, void *to)
{
    return skb_copy_bits(skb, offset, to, len);
}

/**
 * @brief sets the length of the skb and adjust the tail
 * 
 * @param skb, length
 * 
 * @return void
 */
static inline void
__adf_nbuf_set_pktlen(struct sk_buff * skb, uint32_t len)
{
    if (skb->len > len) {
        skb_trim(skb, len);
    }
    else {
        if (skb_tailroom(skb) < len - skb->len) {
            if(unlikely(pskb_expand_head(
                            skb, 0, len - skb->len -skb_tailroom(skb), GFP_ATOMIC))) 
            {
                dev_kfree_skb_any(skb);
                KASSERT(0, ("No enough tailroom for skb, failed to alloc"));
            }
        }     
        skb_put(skb, (len - skb->len));
    }
}

/**
 * @brief sets the protocol value of the skb
 * 
 * @param skb, protocol
 * 
 * @return void
 */
static inline void
__adf_nbuf_set_protocol(struct sk_buff * skb, uint16_t protocol)
{
    skb->protocol = protocol;
}

/**
 * @brief test whether the nbuf is nonlinear or not
 *
 * @param buf
 *
 * @return a_bool_t (TRUE if it is nonlinear or else FALSE)
 */
static inline a_bool_t
__adf_nbuf_is_nonlinear(struct sk_buff *skb)
{
        return skb_is_nonlinear(skb);
}

#endif /*_adf_nbuf_PVT_H */
