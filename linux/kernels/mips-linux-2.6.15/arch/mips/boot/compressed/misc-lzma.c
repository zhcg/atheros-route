/*
 * misc-lzma.c 
 * LZMA decompression module 
 * We set the serial Baud to 115200. Our CPU is Atheros AR7161
 * Modified by Tos Xu 2008
 */

#define         MAX_BAUD                115200
#define		BAUD_DEFAULT		UART16550_BAUD_38400

#define         UART16550_BAUD_38400            38400
#define         UART16550_BAUD_115200           115200
#define         UART16550_PARITY_NONE           0
#define         UART16550_DATA_8BIT             0x3

#define         UART16550_STOP_1BIT             0x0
#define         UART16550_STOP_2BIT             0x4
#define 	UART_BASE_ADDRESS		(volatile int *)0xb8020000

#define UART_IER 1
#define UART_LCR 3
#define UART_DLL 0
#define UART_DLM 1
#define UART_LSR 5
#define UART_RX 0
#define UART_TX 0
#define UART_FCR 2

#define memzero(d, count) (memset((d), 0, (count)))

#define __ptr_t void *
#define NULL ((void *) 0)

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

extern char input_data[];
extern char input_data_end[];

static uch *output_data;

#define _LZMA_PROB32
#include "LzmaDecode.c"
#include <linux/autoconf.h>

#define MAX_LC_PLUS_LP 3
#define LZMA_BUF_SIZE (LZMA_BASE_SIZE + ((LZMA_LIT_SIZE) << (MAX_LC_PLUS_LP)))
static CProb lzma_buf[LZMA_BUF_SIZE];

/* Compute the divisor of console.*/
#ifdef CONFIG_MACH_AR7240
#define AR7240_APB_BASE                 0x18000000  /* 384M */
#define AR7240_PLL_BASE                 AR7240_APB_BASE+0x00050000
#define AR7240_CPU_PLL_CONFIG           AR7240_PLL_BASE
#define PLL_CONFIG_PLL_DIV_SHIFT        0
#define PLL_CONFIG_PLL_DIV_MASK         (0x3ff<< PLL_CONFIG_PLL_DIV_SHIFT)
#define PLL_CONFIG_PLL_REF_DIV_SHIFT    10
#define PLL_CONFIG_PLL_REF_DIV_MASK     (0xf << PLL_CONFIG_PLL_REF_DIV_SHIFT)
#define PLL_CONFIG_AHB_DIV_SHIFT        19
#define PLL_CONFIG_AHB_DIV_MASK         (0x1 << PLL_CONFIG_AHB_DIV_SHIFT)

#define KSEG1			0xa0000000
#define KSEG1ADDR(a)		(((a) & 0x1fffffff) | KSEG1)
#define ar7240_reg_rd(_phys)    (*(volatile unsigned int *)KSEG1ADDR(_phys))

unsigned int
ar7240_sys_frequency()
{
    unsigned int pll, pll_div, ref_div, ahb_div, ddr_div, freq,ahb_freq;

    pll = ar7240_reg_rd(AR7240_CPU_PLL_CONFIG);

    pll_div = 
        ((pll & PLL_CONFIG_PLL_DIV_MASK) >> PLL_CONFIG_PLL_DIV_SHIFT);

    ref_div =
        ((pll & PLL_CONFIG_PLL_REF_DIV_MASK) >> PLL_CONFIG_PLL_REF_DIV_SHIFT);

    ahb_div = 
       (((pll & PLL_CONFIG_AHB_DIV_MASK) >> PLL_CONFIG_AHB_DIV_SHIFT) + 1)*2;

    freq = pll_div * ref_div * 5000000; 

    ahb_freq = freq/ahb_div;

    return ahb_freq; 
}
#endif

/*
 * 0 - kgdb does serial init
 * 1 - kgdb skip serial init
 */
static int remoteDebugInitialized = 0;

unsigned int 
UART8250_read(int offset)
{

	return(*((volatile unsigned int *)((volatile char *)0xb8020000+(offset<<2))));

}

void 
UART8250_write(int offset,int value)
{

       *((volatile unsigned int *)((volatile char *)0xb8020000+(offset<<2))) = value;

}


void 
debugInit(unsigned int baud, unsigned char data, unsigned char parity, unsigned char stop)
{
        /* disable interrupts */
        UART8250_write(UART_IER, 0);

        /* set up buad rate */
        {
                unsigned int divisor = 93;

                /* set DIAB bit */
                UART8250_write(UART_LCR, 0x80);

                /* set divisor: BAUD = 115200 */
#ifdef CONFIG_MACH_AR7240
                divisor = ar7240_sys_frequency()/(16*UART16550_BAUD_115200);
#endif
#ifdef CONFIG_MACH_AR7100
                divisor = 93;
#endif		
                UART8250_write(UART_DLL, divisor & 0xff);
                UART8250_write(UART_DLM, (divisor & 0xff00) >> 8);

                /* clear DIAB bit */
                UART8250_write(UART_LCR, 0x0);
        }

        /* set data format */
        UART8250_write(UART_LCR, data | parity | stop);
}


int 
getDebugChar(void)
{
        if (!remoteDebugInitialized) {
                remoteDebugInitialized = 1;
                debugInit(BAUD_DEFAULT,
                          UART16550_DATA_8BIT,
                          UART16550_PARITY_NONE, UART16550_STOP_1BIT);
        }

        while ((UART8250_read(UART_LSR) & 01) != 01);
        return UART8250_read(UART_RX);
}


int 
putDebugChar(char byte)
{
        if (!remoteDebugInitialized) {
                remoteDebugInitialized = 1;
                debugInit(BAUD_DEFAULT,
                          UART16550_DATA_8BIT,
                          UART16550_PARITY_NONE, UART16550_STOP_1BIT);
        }
     
	while ((UART8250_read(UART_LSR) & 0x60)!= 0x60);
        UART8250_write(UART_TX,byte);	
        return 1;
}

void 
print_uart (unsigned char * buf)
{
	int i=0;
	unsigned char c;

	c=buf[i++];
	while (c!=0)
	{	

		putDebugChar(c);
		c=buf[i++];
	}
}

static void 
error(char *x)
{
	print_uart("Decompressing error:\n\n");
	print_uart(x);
	print_uart("\n\n -- System halted");
	while(1);	/* Halt */
}




void
decompress_kernel(void)
{

	print_uart("Decompressing kernel...\r\n");

	int result;
	unsigned int input_size, output_size;
	unsigned char *p;
	CLzmaDecoderState state;
	
	output_data = (char *)LOADADDR;

	p = (unsigned char *) input_data;

	if (LzmaDecodeProperties(&state.Properties, p, LZMA_PROPERTIES_SIZE)
	    != LZMA_RESULT_OK)
		error("bad properties");

	if (LzmaGetNumProbs(&state.Properties) > LZMA_BUF_SIZE)
		error("not enough memory");

	state.Probs = lzma_buf;

	input_size = input_data_end - input_data;

	output_size = p[8];
	output_size = (output_size << 8) + p[7];
	output_size = (output_size << 8) + p[6];
	output_size = (output_size << 8) + p[5];

#define LZMA_HEADER_SIZE 13
	p += LZMA_HEADER_SIZE;
	input_size -= LZMA_HEADER_SIZE;

	result = LzmaDecode(&state, p, input_size, &input_size,
			    output_data, output_size, &output_size);

	if (result != LZMA_RESULT_OK)
		error("decompression error");
	else
		print_uart("Decompressing Kernel.... OK.\r\n");

}


