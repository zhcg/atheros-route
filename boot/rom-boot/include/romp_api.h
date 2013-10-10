/*************************************************************************/
/*  Copyright (c) 2008 Atheros Communications, Inc., All Rights Reserved */
/*                                                                       */
/*  Module Name : romp_api.h     	                                  	 */
/*                                                                       */
/*  Abstract                                                             */
/*      This file contains definition of data structure and interface    */
/*                                                                       */
/*  NOTES                                                                */
/*      None                                                             */
/*                                                                       */
/*************************************************************************/

#ifndef _ROMP_API_H_
#define _ROMP_API_H_

#include <asm/types.h>
#include <dt_defs.h>

#define _COMPRESSED_MAGIC_    0x4c5a /* "LZ" */

struct patch_hdr {
    u16 _flag;    /* LZ */
    u16 _len;
    u32 _cksum;
    u16 _ld_addr;
    u16 _exec_addr;
    u16 _tmp_addr;
    u16 _tmp_len;
};

/******** hardware API table structure (API descriptions below) *************/
struct romp_api {
    void (*_romp_init)(void);
    BOOLEAN (*_romp_download)(void);
    BOOLEAN (*_romp_install)(void);
    BOOLEAN (*_romp_decode)(struct patch_hdr *);
};

/************************* EXPORT function ***************************/
void romp_module_install(struct romp_api *tbl);

#endif	// end of _UART_API_H_

