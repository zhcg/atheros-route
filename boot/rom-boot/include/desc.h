/************************************************************************/
/*  Copyright (c) 2000-2004  ZyDAS Technology Corporation               */
/*                                                                      */
/*  Module Name : desc_def.h                                            */
/*                                                                      */
/*  Abstract                                                            */
/*      This module contains DMA descriptor related definitions.        */
/*                                                                      */
/*  NOTES                                                               */
/*      None                                                            */
/*                                                                      */
/************************************************************************/

#ifndef _DESC_DEFS_H
#define _DESC_DEFS_H

#if 0
#define BIG_ENDIAN

struct zsDmaDesc
{
#ifdef BIG_ENDIAN

    volatile u16_t      ctrl;       // Descriptor control
    volatile u16_t      status;     // Descriptor status

    volatile u16_t      totalLen;   // Total length
    volatile u16_t      dataSize;   // Data size

#else
    volatile u16_t      status;     // Descriptor status
    volatile u16_t      ctrl;       // Descriptor control
    volatile u16_t      dataSize;   // Data size
    volatile u16_t      totalLen;   // Total length
#endif
    struct zsDmaDesc*   lastAddr;   // Last address of this chain
    volatile u32_t      dataAddr;   // Data buffer address
    struct zsDmaDesc*   nextAddr;   // Next TD address
};
#endif

/* Tx5 Dn Rx Up Int */
#if 0
#define ZM_TERMINATOR_NUMBER_B  8

#if ZM_BAR_AUTO_BA == 1
#define ZM_TERMINATOR_NUMBER_BAR 	1
#else
#define ZM_TERMINATOR_NUMBER_BAR 	0
#endif

#if ZM_INT_USE_EP2 == 1
#define ZM_TERMINATOR_NUMBER_INT 	1
#else
#define ZM_TERMINATOR_NUMBER_INT 	0
#endif

#define ZM_TX_DELAY_DESC_NUM	16
#define ZM_TERMINATOR_NUMBER (8+ZM_TERMINATOR_NUMBER_BAR + \
				ZM_TERMINATOR_NUMBER_INT + \
				ZM_TX_DELAY_DESC_NUM)


#define ZM_BLOCK_SIZE           (256+64)
#define ZM_DESCRIPTOR_SIZE      (sizeof(struct zsDmaDesc))
#endif

//#define ZM_FRAME_MEMORY_BASE    0x100000

/* Erro code */
#define ZM_ERR_FS_BIT           1
#define ZM_ERR_LS_BIT           2
#define ZM_ERR_OWN_BITS         3
#define ZM_ERR_DATA_SIZE        4
#define ZM_ERR_TOTAL_LEN        5
#define ZM_ERR_DATA             6
#define ZM_ERR_SEQ              7
#define ZM_ERR_LEN              8

/* Status bits definitions */
/* Own bits definitions */
#define ZM_OWN_BITS_MASK        0x3
#define ZM_OWN_BITS_SW          0x0
#define ZM_OWN_BITS_HW          0x1
#define ZM_OWN_BITS_SE          0x2

/* Control bits definitions */
/* First segament bit */
#define ZM_LS_BIT               0x100
/* Last segament bit */
#define ZM_FS_BIT               0x200

#if 0
struct zsDmaQueue
{
    struct zsDmaDesc* head;
    struct zsDmaDesc* terminator;
};
#endif

struct zsDmaQueue;
struct szDmaDesc;

extern struct zsDmaDesc* zfDmaGetPacket(struct zsDmaQueue* q);
extern void zfDmaReclaimPacket(struct zsDmaQueue* q, struct zsDmaDesc* desc);
extern void zfDmaPutPacket(struct zsDmaQueue* q, struct zsDmaDesc* desc);

#endif /* #ifndef _DESC_DEFS_H */
