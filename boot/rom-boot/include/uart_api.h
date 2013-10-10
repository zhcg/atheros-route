#ifndef _UART_API_H
#define _UART_API_H

#include <asm/types.h>
#include "hs_uart_reg.h"

struct uart_api {
    void (*_uart_init)(void);
    u8 (*_uart_getc)(void);
    void (*_uart_putc)(u8 byte);
    void (*_uart_puts)(const char *s);
    void (*_uart_putu8)(u8 byte);
    void (*_uart_putu32)(u32 value);
};

void 
uart_module_install(struct uart_api *api);

#endif

