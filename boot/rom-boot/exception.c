/*************************************************************************/
/*  Copyright (c) 2008 Atheros Communications, Inc., All Rights Reserved */
/*                                                                       */
/*  Module Name : exception.c                                            */
/*                                                                       */
/*  Abstract                                                             */
/*      This file contains exception handling code.                      */
/*                                                                       */
/*  NOTES                                                                */
/*      None                                                             */
/*                                                                       */
/*************************************************************************/

#include <asm/addrspace.h>
#include <asm/types.h>
#include <asm/mipsregs.h>
#include <config.h>
#include <ar7240_soc.h>
#include "wasp_api.h"
#include "exception_api.h"

struct exc_data {
    unsigned int    cp0_status;
    unsigned int    cp0_cause;   
    unsigned int    cp0_epc;
    unsigned int    cp0_badvaddr;
    unsigned int    general_reg[32];
};

char *ExcCode_Desc[32] = {
    "Interrupt",                                    /* 00 */
    "TLB modification exception",                   /* 01 */
    "TLB(load or instruction fetch)",               /* 02 */
    "TLB(store)",                                   /* 03 */
    "Address error(load or instruction fetch)",     /* 04 */
    "Address error(store)",                         /* 05 */
    "Bus error(instruction fetch)",                 /* 06 */
    "Bus error(data reference: load or store)",     /* 07 */
    "Syscall",                                      /* 08 */
    "Breakpoint",                                   /* 09 */
    "Reserved instruction",                         /* 10 */
    "Coprocessor Unusable",                         /* 11 */
    "Arithmetic Overflow",                          /* 12 */
    "Trap",                                         /* 13 */
    "",                                             /* 14 */ 
    "Floating point",                               /* 15 */
    "Coprocessor 2 implementation specific",        /* 16 */
    "CorExtend Unusable",                           /* 17 */
    "Precise Coprocessor 2",                        /* 18 */
    "",                                             /* 19 */
    "",                                             /* 20 */
    "",                                             /* 21 */
    "",                                             /* 22 */
    "Ref to WatchHi,WatchLo",                       /* 23 */
    "Machine checkcore",                            /* 24 */
    "",                                             /* 25 */
    "",                                             /* 26 */
    "",                                             /* 27 */
    "",                                             /* 28 */
    "",                                             /* 29 */
    "Cache error",                                  /* 30 */
    ""                                              /* 31 */                                         
};

/*
 * Exception handler
 */
void exception_handler(void)
{  
    unsigned int        val, i;
    unsigned int        *val_ptr;
    struct exc_data     *exc_ptr = (struct exc_data *)(CFG_SRAM_BASE + CFG_INIT_SP_OFFSET - CFG_STACK_SIZE);
    
    val = exc_ptr->cp0_cause;
    val &= CAUSEF_EXCCODE;
    val >>= CAUSEB_EXCCODE;    
    
    A_PRINTF("ExcCode %02x -> %s\n", val, ExcCode_Desc[val]);    
    A_PRINTF("EPC %08x, BadVAddr %08x, Cause %08x, Status %08x\n", exc_ptr->cp0_epc, exc_ptr->cp0_badvaddr, exc_ptr->cp0_cause, exc_ptr->cp0_status);
    A_PRINTF("\n");
    A_PRINTF("R0 (r0) = %08x  R8 (t0) = %08x  R16(s0) = %08x  R24(t8) = %08x\n", exc_ptr->general_reg[0], exc_ptr->general_reg[8], exc_ptr->general_reg[16], exc_ptr->general_reg[24]);
    A_PRINTF("R1 (at) = %08x  R9 (t1) = %08x  R17(s1) = %08x  R25(t9) = %08x\n", exc_ptr->general_reg[1], exc_ptr->general_reg[9], exc_ptr->general_reg[17], exc_ptr->general_reg[25]);
    A_PRINTF("R2 (v0) = %08x  R10(t2) = %08x  R18(s2) = %08x  R26(k0) = %08x\n", exc_ptr->general_reg[2], exc_ptr->general_reg[10], exc_ptr->general_reg[18], exc_ptr->general_reg[26]);
    A_PRINTF("R3 (v1) = %08x  R11(t3) = %08x  R19(s3) = %08x  R27(k1) = %08x\n", exc_ptr->general_reg[3], exc_ptr->general_reg[11], exc_ptr->general_reg[19], exc_ptr->general_reg[27]);
    A_PRINTF("R4 (a0) = %08x  R12(t4) = %08x  R20(s4) = %08x  R28(gp) = %08x\n", exc_ptr->general_reg[4], exc_ptr->general_reg[12], exc_ptr->general_reg[20], exc_ptr->general_reg[28]);
    A_PRINTF("R5 (a1) = %08x  R13(t5) = %08x  R21(s5) = %08x  R29(sp) = %08x\n", exc_ptr->general_reg[5], exc_ptr->general_reg[13], exc_ptr->general_reg[21], exc_ptr->general_reg[29]);
    A_PRINTF("R6 (a2) = %08x  R14(t6) = %08x  R22(s6) = %08x  R30(fp) = %08x\n", exc_ptr->general_reg[6], exc_ptr->general_reg[14], exc_ptr->general_reg[22], exc_ptr->general_reg[30]);
    A_PRINTF("R7 (a3) = %08x  R15(t7) = %08x  R23(s7) = %08x  R31(ra) = %08x\n", exc_ptr->general_reg[7], exc_ptr->general_reg[15], exc_ptr->general_reg[23], exc_ptr->general_reg[31]);
        
    val = exc_ptr->general_reg[29];
    if ( !(val % 4) && (val < CFG_SRAM_BASE + CFG_INIT_SP_OFFSET) && (val >= CFG_SRAM_BASE + CFG_INIT_SP_OFFSET - CFG_STACK_SIZE) ) {
        for (i = 0; val < CFG_SRAM_BASE + CFG_INIT_SP_OFFSET; val += 4, i++) {
            if (!(i %4))
                A_PRINTF("\n[%08x]", val);           
            val_ptr = (unsigned int *)val;
            A_PRINTF("  %08x", *val_ptr);
        }
    }

    while (1)   ;    
}
