#include <dv_dbg.h>
#include <asm/addrspace.h>
#include <asm/types.h>
#include <config.h>
#include <ar7240_soc.h>
#include <uart_api.h>
#include <apb_map.h>

#define		REG_OFFSET		4
//#define nallen 1

/* === END OF CONFIG === */

/* register offset */
#define OFS_RCV_BUFFER		(0*REG_OFFSET)
#define OFS_TRANS_HOLD		(0*REG_OFFSET)
#define OFS_SEND_BUFFER		(0*REG_OFFSET)
#define OFS_INTR_ENABLE		(1*REG_OFFSET)
#define OFS_INTR_ID		(2*REG_OFFSET)
#define OFS_DATA_FORMAT		(3*REG_OFFSET)
#define OFS_LINE_CONTROL	(3*REG_OFFSET)
#define OFS_MODEM_CONTROL	(4*REG_OFFSET)
#define OFS_RS232_OUTPUT	(4*REG_OFFSET)
#define OFS_LINE_STATUS		(5*REG_OFFSET)
#define OFS_MODEM_STATUS	(6*REG_OFFSET)
#define OFS_RS232_INPUT		(6*REG_OFFSET)
#define OFS_SCRATCH_PAD		(7*REG_OFFSET)

#define OFS_DIVISOR_LSB		(0*REG_OFFSET)
#define OFS_DIVISOR_MSB		(1*REG_OFFSET)

#define MY_WRITE(y, z)		((*((volatile u32*)(y))) = z)
#define UART16550_READ(y)	ar7240_reg_rd((AR7240_UART_BASE+y))
#define UART16550_WRITE(x, z)	ar7240_reg_wr((AR7240_UART_BASE+x), z)

void serial_init(void)
{
    u32 div,val;
    u32 bs_reg;

    DV_DBG_RECORD_LOCATION(SERIAL_C); // Location Pointer

    bs_reg = ar7240_reg_rd(WASP_BOOTSTRAP_REG);
    if ((bs_reg & WASP_REF_CLK_25) == 0) {
        div = (25 * 1000000) / (16 * CONFIG_BAUDRATE);
    } else {
        div = (40 * 1000000) / (16 * CONFIG_BAUDRATE);
    }

    /*
     * Configure the 22 gpio pins as output. Skip pins 0 - 3,
     * 5 - 10. They have the following fixed functions.
     *    ejtag_tck, ejtag_tdi, ejtag tdo, ejtag tms, spi_cs,
     *    spi_clk, spi_miso, spi_mosi, uart sin, uart sout
     *
     * For Wasp's OE register
     *		0 -> config as o/p	1 -> config as i/p
     */
    val = ar7240_reg_rd(GPIO_OE_ADDRESS) & (~0xcff810u);
    MY_WRITE(GPIO_OE_ADDRESS, val);

    val = ar7240_reg_rd(0xb8040008) | 0xcff810u;
    MY_WRITE(0xb8040008, val);

    val = ar7240_reg_rd(0xb8040028);
    MY_WRITE(0xb8040028,(val | 0x8402));

    MY_WRITE(0xb8040008, 0x2f);

    /*
     * set DIAB bit
     */
    UART16550_WRITE(OFS_LINE_CONTROL, 0x80);

    /* set divisor */
    UART16550_WRITE(OFS_DIVISOR_LSB, (div & 0xff));
    UART16550_WRITE(OFS_DIVISOR_MSB, ((div >> 8) & 0xff));

    /* clear DIAB bit*/
    UART16550_WRITE(OFS_LINE_CONTROL, 0x00);

    /* set data format */
    UART16550_WRITE(OFS_DATA_FORMAT, 0x3);

    UART16550_WRITE(OFS_INTR_ENABLE, 0);

    DV_DBG_RECORD_LOCATION(SERIAL_C); // Location Pointer
}

int serial_tstc (void)
{
    return(UART16550_READ(OFS_LINE_STATUS) & 0x1);
}

u8 serial_getc(void)
{
    while(!serial_tstc());

    return UART16550_READ(OFS_RCV_BUFFER);
}


void serial_putc(u8 byte)
{
    if (byte == '\n') serial_putc ('\r');

    while (((UART16550_READ(OFS_LINE_STATUS)) & 0x20) == 0x0);
    UART16550_WRITE(OFS_SEND_BUFFER, byte);
}

void serial_setbrg (void)
{
}

void serial_puts (const char *s)
{
	while (*s)
	{
		serial_putc (*s++);
	}
}

char u8_to_char(u8 byte)
{
	char ret;

	if (byte <= 9) //if((0<=byte) && (9>=byte))
	{
		ret = byte + '0';
	}
	else  // 10->A
	{
		ret = byte-10+'A';
	}

	return ret;
}

void serial_putu8(u8 byte)
{
	u8 uA,uB;
	char cA,cB;
	uA = byte>>4;
	uB = byte % 16;

	cA = u8_to_char(uA);
	cB = u8_to_char(uB);

	serial_putc (' ');
	serial_putc (cA);
	serial_putc (cB);
}

void serial_putu32(u32 value)
{
#if 0
	serial_putu8(value>>24);
	serial_putu8(value>>16);
	serial_putu8(value>>8);
	serial_putu8(value);
	serial_putc ('\n');
#endif
	char 	hex[] = "0123456789abcdef";
	int	i, j;
	char	str[16] = "0x";

	for (j = 2, i = (sizeof(u32) * 8 / 4) - 1; i >= 0; j++, i--) {
		str[j] = hex[(value >> (i * 4)) & 0xf];
	}
	str[10] = 0;
	serial_puts(str);
}

void uart_module_install(struct uart_api *api)
{
    api->_uart_init = serial_init;
    api->_uart_getc = serial_getc;
    api->_uart_putc = serial_putc;
    api->_uart_puts = serial_puts;
    api->_uart_putu8 = serial_putu8;
    api->_uart_putu32 = serial_putu32;
}
