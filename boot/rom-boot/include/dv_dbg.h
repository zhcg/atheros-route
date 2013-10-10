#ifndef _DV_DBG_H
#define _DV_DBG_H

#include <config.h>

#define START_S             0x0
#define LOWLEVEL_INIT_S     0x1
#define CACHE_S             0x2
#define MAIN_C              0x3
#define SERIAL_C            0x4
#define OTP_C               0x5
#define USB_DEV_C           0x6
#define USB_DEV_C_FW2       0x7
#define START_S_FW2         0x8
#define USB_SUSPEND_C_FW2   0x9
#define CLOCK_C             0xa
#define MDIO_C              0xb
#define ROMP_C              0xc
#define PRINTF_C            0xd
#define NAND_C              0xe


/* 0xB8060004 = Timer0 reload value */
#ifdef DV_DBG_FUNC

    /* Record current location of program (one for C program , the other for assembly) */
    #define DV_DBG_RECORD_LOCATION(NAME) \
    (*(volatile unsigned long *)0xb8060004 = ((NAME << 16) | __LINE__))
    #define DV_DBG_RECORD_LOCATION_ASM(NAME) \
        lui s5, NAME ; \
        ori s5, s5, __LINE__ ; \
        li s4, 0xb8060004 ; \
        sw s5, 0(s4);         

    /* Record data value (for C program only, X can be either a variable or a literal) */
    #define DV_DBG_RECORD_DATA(X) \
    (*(volatile unsigned long *)0xb8060004 = (X))
                
    /* Record register value (for assembly only, X must be a register name) */
    #define DV_DBG_RECORD_REGISTER_ASM(X) \
        li s4, 0xb8060004 ; \
        sw X, 0(s4); 
            
#else /* DV_DBG_FUNC */

    /* Record current location of program (one for C program , the other for assembly) */
    #define DV_DBG_RECORD_LOCATION(X)
    #define DV_DBG_RECORD_LOCATION_ASM(X)

    /* Record data value (for C program only, X can be either a variable or a literal) */
    #define DV_DBG_RECORD_DATA(X)

    /* Record register value (for assembly only, X must be a register name) */
    #define DV_DBG_RECORD_REGISTER_ASM(X)    
    
#endif /* DV_DBG_FUNC */

#endif /* _DV_DBG_H */
