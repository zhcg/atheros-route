/*************************************************************************/
/*  Copyright (c) 2008 Atheros Communications, Inc., All Rights Reserved */
/*                                                                       */
/*  Module Name : rompatch.c                                  	 */
/*                                                                       */
/*  Abstract                                                             */
/*      This file contains rom code patch mechanism, patch code is       */
/*      offline generated, and replace the indirect table function as we */
/*      need to patch. 												     */
/*                                                                       */
/*  NOTES                                                                */
/*      None                                                             */
/*                                                                       */
/*************************************************************************/

#include "dv_dbg.h"
#include "wasp_api.h"
#include "romp_api.h"
#include "LzmaWrapper.h"
#include "mem_use.h"

#define _DEBUG_ 0

#if _DEBUG_
#define _DEBUG_PRINTF_ 1
#define PATCH_IMG_LOC 0xbd004000     /* img loaded by t32 */
#endif

/* patch install entry  */
void (*patch_start)(void);

/************************************/
#if _DEBUG_
void init_patch(struct patch_hdr *dst, struct patch_hdr *src)
{
    dst->_flag      = src->_flag;
    dst->_len       = src->_len;
    dst->_cksum     = src->_cksum;
    dst->_ld_addr   = src->_ld_addr;
    dst->_exec_addr = src->_exec_addr;
    dst->_tmp_addr  = src->_tmp_addr;
    dst->_tmp_len   = src->_tmp_len;
}

/*! _patch_dump
 * dump patch
 *
 */
void _patch_dump( struct patch_hdr *patch)
{
#if _DEBUG_PRINTF_
    A_PRINTF("\tFlag: %x\n\r", patch->_flag);
    A_PRINTF("\tCRC: 0x%08x\n\r", patch->_cksum);
    A_PRINTF("\tsize: %d bytes\n\r", patch->_len);
    A_PRINTF("\tld_addr: 0x%08x\n\r", (u32)(patch->_ld_addr|SRAM_BASE_ADDR));
    A_PRINTF("\tentry_addr: 0x%08x\n\r", (u32)(patch->_exec_addr|SRAM_BASE_ADDR));
    A_PRINTF("\ttmp_addr: 0x%08x\n\r", (u32)(patch->_tmp_addr|SRAM_BASE_ADDR));
    A_PRINTF("\ttmp_len: %d bytes\n\r", (u32)patch->_tmp_len);
#endif
}

#endif
/************************************/


/*! romp_decode
 * a. check integrity of the patch
 * b. decompress patch (if "LZ" is on)
 * c. copy to target address
 *
 * return TRUE if patch exist, FALSE if not
 */
LOCAL
BOOLEAN romp_decode(struct patch_hdr *hBuf)
{
    BOOLEAN retVal = FALSE;
    u32 dst_addr   = ((u32)(hBuf->_ld_addr)|SRAM_BASE_ADDR);
    s32 dst_len    = (s32)(hBuf->_len);
    u32 src_addr   = 0x0;
    s32 src_len    = 0x0;
    u32 tmp_addr   = 0x0;
    s32 tmp_len    = 0x0;
    u32 cksum      = 0x0;

    cksum = A_CHKSUM((u32*)dst_addr, dst_len);

    /* check the checksum of the image */
    if ( hBuf->_cksum != cksum ) {
        goto ERR_DONE;
    }

    if( hBuf->_flag == (u16)_COMPRESSED_MAGIC_ ) {
        src_addr = ((u32)(hBuf->_tmp_addr)|SRAM_BASE_ADDR);
        src_len  = (s32)(hBuf->_len);
        tmp_addr = ((u32)(hBuf->_tmp_addr)|SRAM_BASE_ADDR)+(u32)src_len+(4-src_len%4);
        tmp_len  = ((s32)(hBuf->_tmp_len))-(src_len+(4-src_len%4));

        A_MEMCPY((void *)src_addr, (void *)dst_addr, src_len);

        if( lzma_inflate((u8*)src_addr, src_len,(u8*)dst_addr, &dst_len, (u8*)tmp_addr, tmp_len) ) {
    	      goto ERR_DONE;
        }
    }

    patch_start = (void *)((u32)(hBuf->_exec_addr)|SRAM_BASE_ADDR);
    retVal = TRUE;

ERR_DONE:
    return retVal;    
}


/*! romp_install
 * 
 * call patch_start() to install the patch code
 */
LOCAL BOOLEAN romp_install(void)
{
    /*
     * sainty check if patch_start is not located at sram address range ?
     */
    if( ((u32)patch_start&SRAM_BASE_ADDR)==SRAM_BASE_ADDR )
        patch_start();
    
    return TRUE;
}

/*! romp_download
 * a. download the patch from otp
 * b. sanity check of len, dowload addr.. etc
 * c. call decode
 * d. install the patch
 * 
 *
 */
LOCAL BOOLEAN romp_download()
{
    BOOLEAN retVal = FALSE;
    struct patch_hdr pHdr;
    u32 dst_addr = 0x0;
    u32 src_addr = 0x0;
    s32 src_len  = 0x0;

    A_MEMSET((void*)&pHdr, 0x0, sizeof(struct patch_hdr));

#if _DEBUG_
    src_addr = PATCH_IMG_LOC;

    init_patch(&pHdr, (struct patch_hdr *)(src_addr));
    _patch_dump(&pHdr);
#else
    /* read patch offset */
    src_addr =(u32) A_OTP_READ_PATCH();

    if(!src_addr)
        goto ERR_DONE;

    DV_DBG_RECORD_LOCATION(ROMP_C);
    DV_DBG_RECORD_DATA(src_addr);
    
    src_addr += OTP_BASE;
    
    /* get patch header */
    if( 0xdeadbeef == A_OTP_COPY((u32 *)&pHdr, (u32 *)(src_addr), sizeof(struct patch_hdr)) )
        goto ERR_DONE;
#endif

    dst_addr = (u32)(pHdr._ld_addr)|SRAM_BASE_ADDR;
    src_len = (s32)(pHdr._len);

   /* remove patch_hdr and point to patch data */
    src_addr = src_addr+sizeof(struct patch_hdr);

    DV_DBG_RECORD_LOCATION(ROMP_C);
    DV_DBG_RECORD_DATA(dst_addr);
    DV_DBG_RECORD_DATA(src_len);
    DV_DBG_RECORD_DATA(src_addr);

    /* 
     * 1. len > 0
     * 2. ld_addr reasonable? (ld_addr >0x0 && (ld_addr+len)<0x8000)
     * 3. ld_addr==0 
     * etc............
     */
    if( (src_len==0) || (dst_addr==0) || ((src_len)>=0x8000) ) {
        DV_DBG_RECORD_LOCATION(ROMP_C);
#if _DEBUG_PRINTF_
        A_PRINTF("patch len:     %d\n\r", src_len);
        A_PRINTF("patch addr: %d\n\r", dst_addr);
#endif
        goto ERR_DONE;
    }

    /* read patch image from (src_addr) to {ld_addr, len} based on patch header */
#if _DEBUG_
    A_MEMCPY((u32*)dst_addr, (u32*)src_addr, src_len);
#else
    if( 0xdeadbeef == A_OTP_COPY((u32*)(dst_addr), (u32*)(src_addr), src_len) )
        goto ERR_DONE;
#endif

    if( !A_ROMP_DECODE(&pHdr) )
        goto ERR_DONE;
    
    A_ROMP_INSTALL();

    retVal = TRUE;
ERR_DONE:
    return retVal;

}

/*! romp_init
 * dummy init
 */
LOCAL void romp_init(void)
{
}

void
romp_module_install(struct romp_api *api)
{
    api->_romp_init         = romp_init;
    api->_romp_download     = romp_download;
    api->_romp_install      = romp_install;
    api->_romp_decode       = romp_decode;

}
