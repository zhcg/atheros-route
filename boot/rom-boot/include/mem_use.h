#ifndef  MEM_USE_H
#define  MEM_USE_H

/*-----------------------------------------------------
 * Memory Usage
 ----------------------------------------------------*/

/*
 *  SRAM 32K memory layout
 *
 *	            _____________ <- 0xBD00_0000 (Start of SRAM)
 *			    |            |<- 0xBD00_0100 (Susped/Resume text will be copied to here)
 * 	    4K      |  ROM Data  |
 * 	            |            |
 * 			    ------------- <- 0xBD00_1000
 * 	   1024B    |  ENDPT QH  |               (USB QH, need (64*12=768=0x300))
 * 			    --------------<- 0xBD00_1300
 *     128B     |  DTD       |               (USB DTD, need (64*2=128=0x80, 1 for Endpt 0 Tx, 1 for Endpt 0 Rx)
 * 			    --------------<- 0xBD00_1380
 *    3200B	    |  Not Used  |
 * 			    --------------<- 0xBD00_2000
 * 			    |            |
 * 	    8K	    |  4K Buf*2  |               (USB Buf, need 4K Buf*2, 1 for Endpt0 Tx, 1 for Endpt 0 Rx)
 * 			    |            |
 *              |------------|<- 0xBD00_4000
 * 			    |            |
 * 		8K	    |  Not Used  |
 * 			    |            |
 * 			    --------------<- 0xBD00_6000
 * 	  7K-16B    |            |
 *              |  1st FW DL |
 *              |            |
 * 			    --------------<- 0xBD00+7bF0
 * 		1K	    |  Stack     |
 * 		        |            |<- 0xBD00_7FF0  (Stack, Warm Start is Labelled here)
 *              --------------<- 0xBD00_7FFF  (End of SRAM)
 *
 *                                                                             (0xBD00_2000)
 *                                                                           -------------
 *                                                                           | 4K buffer  |
 *                             EP_LIST_ADDR(0xBD00_1000)      DTD_BASE_ADDR / -------------
 *  ENDPOINTLISTADDR ----->    -------------------------      --------  (0xBD00_1300)
 *  (REG 0x158: EP_LIST_ADDR)  |  Endpoint QH 0 - Out   | --> | DTD 0 |
 *                             -------------------------      --------
 *                             |  Endpoint QH 0 - In    | --> | DTD 1 |
 *                             -------------------------      --------  (0xBD00_1340)
 *                             |  Endpoint QH 1 - Out   |              \ -------------
 *                             --------------------------                | 4K buffer |
 *                             |  Endpoint QH 1 - In    |                -------------
 *                             --------------------------                (0xBD00_3000)
 *                             |  Endpoint QH 2 - Out   |
 *                             --------------------------
 *                             |  Endpoint QH 2 - In    |
 *                             --------------------------
 *
 *
 *
 */

#define  SRAM_BASE_ADDR          0xBD000000

/********************************************/
/*           USB used SRAM define           */
/********************************************/
#define  SRAM_START_TYPE_ADDR    0xBD007FF0
#define  EP0_OUT_LIST_ADDR   	 (0x1000+SRAM_BASE_ADDR)  //QH 64 bytes
#define  EP0_IN_LIST_ADDR   	 (EP0_OUT_LIST_ADDR+0x40)

#define  DTD0_ADDR              (0x40*6*2+EP0_OUT_LIST_ADDR)
#define  DTD1_ADDR				 (DTD0_ADDR+0x40*1)             //Endpoint 0 Tx

#define  BUF0_BASE_ADDR          ((0x2000)+SRAM_BASE_ADDR)
#define  BUF1_BASE_ADDR          ((0x3000)+SRAM_BASE_ADDR)


#endif

