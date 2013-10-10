//#include "cmnos_api.h"
#include "asm/types.h"
#include "asm/addrspace.h"
#include "ar7240_soc.h"

#include "gmac_fwd.h"
#include <wasp_api.h>
#include <apb_map.h>

LOCAL void
mdio_init_device(void)
{
	a_uint32_t mask;

	ar7240_reg_rmw_clear(AR7240_RESET, AR7240_RESET_GE0_MDIO);
	udelay(100);

	//phy address setting
	ar7240_reg_wr(MDIO_PHY_ADDR_ADDRESS, 0x7);

	// mdc - 4	mdio - 17
	mask = ar7240_reg_rd(GPIO_IN_ENABLE3_ADDRESS) &
		~(GPIO_IN_ENABLE3_BOOT_EXT_MDC_MASK |
			GPIO_IN_ENABLE3_BOOT_EXT_MDO_MASK);
	mask |= GPIO_IN_ENABLE3_BOOT_EXT_MDC_SET(4) |
		GPIO_IN_ENABLE3_BOOT_EXT_MDO_SET(17);
	ar7240_reg_wr(GPIO_IN_ENABLE3_ADDRESS, mask);

	/*
	 * GPIO 4 is input for MDC,
	 */
	mask = ar7240_reg_rd(GPIO_OE_ADDRESS);
	mask |= ((1 << 4) & ~(1 << 17) );
	ar7240_reg_wr(GPIO_OE_ADDRESS, mask);

	// GPIO 17 mdio_out_en select
	mask = ar7240_reg_rd(GPIO_OUT_FUNCTION4_ADDRESS) &
		~(GPIO_OUT_FUNCTION4_ENABLE_GPIO_17_MASK);
	mask |= GPIO_OUT_FUNCTION4_ENABLE_GPIO_17_SET(2);
	ar7240_reg_wr(GPIO_OUT_FUNCTION4_ADDRESS, mask);
}

LOCAL int
mdio_wait_for_lock(void)
{
    volatile a_uint16_t val;

    val = mii_reg_read_32(MDIO_REG_TO_OFFSET(mdio_lock_reg));
    while (! (val & MDIO_OWN_TGT)) {
        val = mii_reg_read_32(MDIO_REG_TO_OFFSET(mdio_lock_reg));
    }
    //A_PRINTF("mdio_wait_for_lock - len %x %x\n", val);
    return ((val&0xff00) >> 8);

}

LOCAL void
mdio_release_lock(unsigned char extra_flags)
{
   mii_reg_write_32(MDIO_REG_TO_OFFSET(mdio_lock_reg), (extra_flags << 8) | MDIO_OWN_HST);
}

LOCAL int
mdio_read_block(unsigned char *ptr, int len)
{
    int  j=0, next_read_reg=1;
    a_uint16_t val;

    for (j=0;  j < len ; ) {
        val = mii_reg_read_32(MDIO_REG_TO_OFFSET(next_read_reg));
        //A_PRINTF("mdio_read_block, -RD- %x- %x\n", next_read_reg, val);
        if ((j == len -1) && (len % 2)) {
            /* last odd word */
            ptr[j]=(val & 0x00ff);
        } else {
             ptr[j] = (val & 0xff00 ) >> 8;
             ptr[j+1] = (val & 0x00ff);
        }
        j+=2;
        next_read_reg+=1;
    }

    return 0;
}

/*!
 *  mdio copy bytes from fifo
 *
 *     to: buffer to store the data
 * length: length of data to read
 *
 * return: the size of exactly the data we read, supposed to be same with length
 *
 */
LOCAL int
mdio_copy_bytes(unsigned char* to, int length)
{
    int cwindex=0;
    int ilen=0;

    A_PRINTF("started receiving bytes %d\n", length);
    while ( cwindex < length) {
        ilen = A_MDIO_WAIT_LOCK();
        //A_PRINTF("Bytes to read %d\n", ilen);
        A_MDIO_READ_BLOCK( to + cwindex, ilen);
        cwindex += ilen;
        A_MDIO_RELEASE_LOCK(0);
    }
    A_PRINTF("completed receiving bytes\n");

    return cwindex;
}

LOCAL unsigned int
mdio_compute_cksum(unsigned int *ptr, int len)
{
    unsigned int sum=0x0;
    int i=0;

    for (i=0; i<len; i++)
        sum += ~ptr[i];

    return sum;
}

#ifdef FORCE_CHECKSUM_ERROR
int induce_count=0;
#endif


LOCAL int
mdio_get_fw_image(mdio_bw_exec_t *fw_bw_state)
{
    int rdlen;

    A_MDIO_RELEASE_LOCK(0); /* Host will be waiting initially */

    rdlen = A_MDIO_WAIT_LOCK();
    A_PRINTF("Firmware Download length %d\n", rdlen);

    /* first write should tell to where to copy and how many to copy */
    A_MDIO_READ_BLOCK((unsigned char *)fw_bw_state, rdlen);
    A_PRINTF("Firmware Exec Address %x\n", fw_bw_state->exec_address);

    fw_bw_state->current_wr_ptr = (unsigned char *)fw_bw_state->start_address;
    A_MDIO_RELEASE_LOCK(0);

    /* host would be sending the firmware checksum now */
    rdlen = A_MDIO_WAIT_LOCK();
    A_MDIO_READ_BLOCK((unsigned char *)&fw_bw_state->checksum, rdlen);
    A_PRINTF("Firmware checksum 0x%x\n", fw_bw_state->checksum);
    A_MDIO_RELEASE_LOCK(0);

    while (!fw_bw_state->fwd_state) {
        A_MDIO_COPY_BYTES((unsigned char*)fw_bw_state->start_address,
                               fw_bw_state->length);

        A_MDIO_WAIT_LOCK();
#if FORCE_CHECKSUM_ERROR
        if (induce_count++ < 40)
           for (l=0; l < 100; l++) *((unsigned int*)(fw_bw_state->start_address)+100+l) = 0xff;
#endif
        /* compute checksum and communicate status */
        if (fw_bw_state->checksum != A_MDIO_COMP_CKSUM((unsigned int*)fw_bw_state->start_address, fw_bw_state->length/4)) {
            A_PRINTF("Firmware checksum failed - re negotiating, 0x%x!=0x%x \n", fw_bw_state->checksum, A_MDIO_COMP_CKSUM((unsigned int*)fw_bw_state->start_address, fw_bw_state->length/4));
            fw_bw_state->fwd_state = 0;
            A_MDIO_RELEASE_LOCK(MDIO_FWD_RESET);
            continue;
        } else {
            A_PRINTF("Firmware Download is good \n");
            fw_bw_state->fwd_state=1;
            A_MDIO_RELEASE_LOCK(MDIO_FWD_GOOD);
            /* looks like MDIO write is lost when code is executing */
            rdlen = A_MDIO_WAIT_LOCK();
            if (rdlen & MDIO_FWD_START) {
                A_PRINTF("COMMAND TO START FIRMWARE RECEIVED \n");
            }
            /*A_DELAY_USECS(1000*200); */
            break;
        }
    }
    return 0;
}

void mdio_module_install(struct mdio_api *api)
{
    api->_mdio_init = mdio_init_device;
    api->_mdio_download_fw = mdio_get_fw_image;
    api->_mdio_wait_lock = mdio_wait_for_lock;
    api->_mdio_release_lock = mdio_release_lock;
    api->_mdio_read_block = mdio_read_block;
    api->_mdio_copy_bytes = mdio_copy_bytes;
    api->_mdio_comp_cksum = mdio_compute_cksum;
}
