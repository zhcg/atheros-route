#ifndef _ATH_SPI_SI3217x_H
#define _ATH_SPI_SI3217x_H


#define SPI_DEV_NAME	"spiS1"
#define SPI_SI_REG_READ		1
#define SPI_SI_REG_WRITE	2
#define SPI_SI_RAM_READ		3
#define SPI_SI_RAM_WRITE	4
#define SPI_SI_RESET		5

#define ENABLE	1
#define DISABLE	0

//SPI_CS1
#define ATH_SPI1_CE_LOW			0x50000 
#define ATH_SPI1_CE_HIGH		0x50100	

typedef struct si_ram_data {
	unsigned short sid;
	unsigned char channel;
	unsigned short ramAddr;
	unsigned int data;
} SI3217x_RAM_DATA;

typedef struct si_reg_data {
	unsigned short sid;
	unsigned char channel;
	unsigned char regAddr;
	unsigned char data;
} SI3217x_REG_DATA;


/*
 * primitives
 */

#define ath_be_msb(_val, __i) (((_val) & (1 << (7 - __i))) >> (7 - __i))

#define ath_spi_bit_banger(_byte)	do {				\
	int _i;								\
	for(_i = 0; _i < 8; _i++) {					\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_LOW | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_LOW | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_HIGH | ath_be_msb(_byte, _i));	\
		ath_reg_wr_nf(ATH_SPI_WRITE,				\
			ATH_SPI1_CE_HIGH | ath_be_msb(_byte, _i));	\
	}								\
} while(0)


#define ath_spi_delay_8()	ath_spi_bit_banger(0)


#endif /*_ATH_FLASH_H*/
