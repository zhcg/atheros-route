#include "dt_defs.h"
#include "spi.h"
#include "reg.h"

/* Wait till Transaction busy indication bit in SPI control/status register of Falcon's SPI Flash Controller is clear */
void _sflash_WaitTillTransactionOver(void)
{
    uint32_t        poldata;
    uint32_t        flg;

    //led_on(10);
    //led_off(12);

    do
    {
        poldata = HAL_WORD_REG_READ(SPI_CS_ADDRESS);

        flg = SPI_CS_BUSY_GET(poldata);

    } while (flg != 0x0);
}

/* Wait till Write In Progress bit in Status Register of Serial Flash is clear */
void _sflash_WaitTillNotWriteInProcess(void)
{
    uint32_t        poldata;
    uint32_t        flg;

    //led_on(12);
    //led_off(10);
    
    do
    {
        _sflash_WaitTillTransactionOver();

        HAL_WORD_REG_WRITE( SPI_AO_ADDRESS, SPI_AO_OPC_SET(ZM_SFLASH_OP_RDSR) );
        HAL_WORD_REG_WRITE( SPI_CS_ADDRESS, SPI_CS_TXBCNT_SET(1) | SPI_CS_RXBCNT_SET(1) | SPI_CS_XCNSTART_SET(1) );

        _sflash_WaitTillTransactionOver();

        poldata = HAL_WORD_REG_READ(SPI_D_ADDRESS);
        flg = poldata & ZM_SFLASH_STATUS_REG_WIP;
    } while (flg != 0x0);
}


/************************************************************************/
/* Function to Send WREN(Write Enable) Operation                        */
/************************************************************************/
void _sflash_WriteEnable()
{
    _sflash_WaitTillNotWriteInProcess();

    HAL_WORD_REG_WRITE( SPI_AO_ADDRESS, SPI_AO_OPC_SET(ZM_SFLASH_OP_WREN) );
    HAL_WORD_REG_WRITE( SPI_CS_ADDRESS, SPI_CS_TXBCNT_SET(1) | SPI_CS_RXBCNT_SET(0) | SPI_CS_XCNSTART_SET(1) );

    _sflash_WaitTillTransactionOver();
}

void sflash_write(uint32_t addr, uint32_t len, uint8_t *buf)
{
    uint32_t    s_addr, e_addr;
    uint32_t    reminder, write_byte;
    uint32_t    data_offset;
    uint32_t    next_page_base;
    uint32_t    t_word_data;
    uint32_t    *pBuf = (uint32_t *)buf;

    e_addr = addr + len;
    for (s_addr = addr; s_addr < e_addr; pBuf++)
    {
        next_page_base  = (s_addr - s_addr%ZM_SFLASH_PAGE_SIZE) + ZM_SFLASH_PAGE_SIZE;
        reminder = e_addr - s_addr;

        write_byte = next_page_base - s_addr;

        if (write_byte >= 4)
            write_byte = 4;

        if (write_byte > reminder)
            write_byte = reminder;

        data_offset = s_addr - addr;

        //memcpy(&t_word_data, buf + data_offset, write_byte);
        t_word_data = *pBuf;

        _sflash_WriteEnable();
        _sflash_WaitTillNotWriteInProcess();

        HAL_WORD_REG_WRITE( SPI_AO_ADDRESS, SPI_AO_OPC_SET(ZM_SFLASH_OP_PP) | SPI_AO_ADDR_SET(s_addr) );
        HAL_WORD_REG_WRITE( SPI_D_ADDRESS, SPI_D_DATA_SET(t_word_data) );
        HAL_WORD_REG_WRITE( SPI_CS_ADDRESS, SPI_CS_TXBCNT_SET(4 + write_byte) | SPI_CS_RXBCNT_SET(0) | SPI_CS_XCNSTART_SET(1) );

        _sflash_WaitTillTransactionOver();

        s_addr += write_byte;
    }
}


/************************************************************************/
/* Function to Send Sector/Block/Chip Erase Operation                   */
/************************************************************************/
void sflash_erase(uint32_t erase_type, uint32_t addr)
{
    uint32_t    erase_opcode;
    uint32_t    tx_len;

    if (erase_type == ZM_SFLASH_SECTOR_ERASE)
    {
        erase_opcode = ZM_SFLASH_OP_SE;
        tx_len = 4;
    }
    else if (erase_type == ZM_SFLASH_BLOCK_ERASE)
    {
        erase_opcode = ZM_SFLASH_OP_BE;
        tx_len = 4;
    }
    else
    {
        erase_opcode = ZM_SFLASH_OP_CE;
        tx_len = 1;
    }

    _sflash_WriteEnable();
    _sflash_WaitTillNotWriteInProcess();

    HAL_WORD_REG_WRITE( SPI_AO_ADDRESS, SPI_AO_OPC_SET(erase_opcode) | SPI_AO_ADDR_SET(addr) );
    HAL_WORD_REG_WRITE( SPI_CS_ADDRESS, SPI_CS_TXBCNT_SET(tx_len) | SPI_CS_RXBCNT_SET(0) | SPI_CS_XCNSTART_SET(1) );

#if 0
    /* Do not wait(let it be completed in background) */
    _cmnos_sflash_WaitTillTransactionOver();
#else

    _sflash_WaitTillNotWriteInProcess(); /* Chip Erase takes 80 - 200 seconds to complete */
#endif
}

void sflash_init()
{
    /* Force SPI address size to 24 bits */
    HAL_WORD_REG_WRITE( SPI_CS_ADDRESS, SPI_CS_AUTOSIZ_OVR_SET(2) );
}

#define START_ADDR 0x0
#define FW_SIZE    40*1024
#define FW_BUFFER  0x510000

int main(void)
{
    sflash_erase(ZM_SFLASH_BLOCK_ERASE, 0);

    sflash_write(START_ADDR, FW_SIZE, (uint8_t *)(FW_BUFFER));

    return 0;
}
