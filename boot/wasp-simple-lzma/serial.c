
#define AR7240_APB_BASE     0x18000000  /* 384M */

#define AR7240_UART_BASE    AR7240_APB_BASE+0x00020000
#define AR7240_RESET_BASE   AR7240_APB_BASE+0x00060000

#define AR7240_REV_ID       (AR7240_RESET_BASE + 0x90)

#define REG_OFFSET          4
#define OFS_LINE_STATUS     (5*REG_OFFSET)
#define OFS_SEND_BUFFER     (0*REG_OFFSET)

#define KSEG1               0xa0000000
#define KSEG1ADDR(a)        (((a) & 0x1fffffff) | KSEG1)

#define ar7240_reg_rd(_phys)    \
            (*(volatile unsigned int *)KSEG1ADDR(_phys))
#define ar7240_reg_wr_nf(_phys, _val) \
            ((*(volatile unsigned int *)KSEG1ADDR(_phys)) = (_val))
#define ar7240_reg_wr(_phys, _val) do {     \
            ar7240_reg_wr_nf(_phys, _val);  \
            ar7240_reg_rd(_phys);       \
}while(0);

#define UART16550_READ(y)   ar7240_reg_rd((AR7240_UART_BASE+y))
#define UART16550_WRITE(x, z)   ar7240_reg_wr((AR7240_UART_BASE+x), z)

void (*serial_putc) (char byte);

/* platform serial_putc */
static void _wasp_serial_putc(char byte)
{
    if (byte == '\n') _wasp_serial_putc ('\r');
    while (((UART16550_READ(OFS_LINE_STATUS)) & 0x20) == 0x0);
        UART16550_WRITE(OFS_SEND_BUFFER, byte);
}

static void (*wasp_serial_putc)(char) = (void *) &_wasp_serial_putc;

static void (*hornet_serial_putc)(char) = (void *) 0xbfc00d88;

/* platform macros */
#define AR7240_REV_ID_MASK      0xff0
#define REV_ID_HORNET           0x11
#define REV_ID_WASP             0x12

void serial_init(void)
{
    int revid = (ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK) >> 4;
    switch(revid) {
    case REV_ID_HORNET:
        serial_putc = hornet_serial_putc;
        break;
    case REV_ID_WASP:
        serial_putc = wasp_serial_putc;
        break;
    default:
        /* die */
        break;
    }
}
