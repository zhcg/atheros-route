
#include "printf.h"

/* LZMA settings */
#define P2_UNCOMP_LOAD_ADDR 0x80002000
#define P2_UNCOMP_LOAD_LEN  0x011fe000

#define P2_TEMP_BUF_ADDR 0x81500000
#define P2_TEMP_BUF_LEN  0x00100000

extern int lzma_inflate(unsigned char *source, int s_len, 
    unsigned char *dest, int d_len, 
    unsigned char *propaddr, int propaddrlen);


/* firmware information */
extern unsigned long vmlrd_lzma[];
extern unsigned long vmlrd_lzmaSize;
extern unsigned long vmlrd_lzma_entry;


/* HW related functions/macros */
#define writel(b,addr) (void)((*(volatile unsigned int *) (addr)) = (b))
#define readl(addr) (*(volatile unsigned int *) (addr))

void disable_usb_intr(void)
{
#define BASE_ADDRESS                    0xA0000000
#define CI13320A_USB_BASE               (BASE_ADDRESS+0x1b000000)
#define CI13320A_USB_USBINTR            (CI13320A_USB_BASE + 0x148)
    writel(0, CI13320A_USB_USBINTR);
}


/* functions */
void (*start_kernel) (void);

int main(int argc, char *argv[])
{
#if 1
    unsigned long src_addr   = (unsigned long) vmlrd_lzma;
    unsigned long src_len    = vmlrd_lzmaSize;
    unsigned long dst_addr   = P2_UNCOMP_LOAD_ADDR;
    unsigned long dst_len    = P2_UNCOMP_LOAD_LEN;
    unsigned long tmp_addr   = P2_TEMP_BUF_ADDR;
    unsigned long tmp_len    = P2_TEMP_BUF_LEN;

    /* HW related initializing */
    serial_init();
    disable_usb_intr();

    /* Decompress LZMA */
    printf("Lzma decompressing(addr: %x len: %d)...\n", src_addr, src_len);

    if (lzma_inflate((unsigned char *) src_addr, src_len, 
        (unsigned char *) dst_addr, dst_len, 
        (unsigned char *) tmp_addr, tmp_len)) {
        printf("failed to decompress kernel\n");
        goto hang;
    }

    printf("Jump to kernel(addr: %x)...\n", vmlrd_lzma_entry);

    start_kernel = (void *) vmlrd_lzma_entry;
    start_kernel();

    printf("return from kernel...\n");

hang:
    while(1);
#else
    #define UNCOMP_LOAD_LEN 0x400000
    #define TEMP_BUF_ADDR 0x100000
    unsigned long src_addr   = (unsigned long) vmlrd;
    unsigned long src_len    = vmlrdSize;
    char *dst_addr, *tmp_addr;
    unsigned long dst_len   = UNCOMP_LOAD_LEN;
    unsigned long tmp_len   = TEMP_BUF_ADDR;
    dst_addr = malloc(dst_len);
    tmp_addr = malloc(tmp_len);
    if (!dst_addr || !tmp_addr) {
        printf("out of memory!\n");
        return 0;
    }
    printf("%d %d %d\n", src_len, dst_len, tmp_len);
    lzma_inflate((unsigned char *) src_addr, src_len,
        (unsigned char *) dst_addr, dst_len,
        (unsigned char *) tmp_addr, tmp_len);
    free(dst_addr);
    free(tmp_addr);
#endif

    return 0;
}

