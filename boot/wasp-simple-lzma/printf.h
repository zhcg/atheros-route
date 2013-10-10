#ifndef __PRINTF_H_
#define __PRINTF_H_

extern void serial_init(void);
extern void (*serial_putc)(char byte);
extern int printf(const char *format, ...);

#endif
