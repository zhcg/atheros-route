#ifndef USB_CONFIG_H
#define USB_CONFIG_H

#include "wasp_api.h"
#include "usb_std.h"

/*
 *       This file is for setting of values of descriptors.
 */

#define USB_DEVICE_CLASS        0xFF
#define USB_DEVICE_SUBCLASS     0xFF
#define USB_DEVICE_PROTOCOL     0xFF
#define USB_ID_VENDOR           0x0CF3
#define USB_ID_PRODUCT          0x9340
#define USB_BCD_DEVICE          0x0108
#define USB_LANG_ID             0x0409
#define USB_MAX_POWER           0xFA
#define USB_PHY_BIAS            0x3

//These three values may be different per vendor.
//So it's should be dynamically configured
#define USB_MANUFACTURER        0x10
#define USB_PRODUCT             0x20
#define USB_SERIAL_NUMBER       0x30


typedef struct endpt_config
{
    __u8  address;
    __u8  direction;
    __u8  type;
}ENDPT_CONFIG;

typedef struct usb_config
{
    __u16  idVendor;         //= 0x0CF3;
    __u16  idProduct;        //= 0x9330;
    __u16  iManufacturer;    //= 0x10;
    __u16  iProduct;         //= 0x20;
    __u16  iSerialNumber;    //= 0x30;
    __u8   bMaxPower;        //= 0xFA;
    __u8   bUSBPhyBias;      //= 0x3;
} USB_CONFIG;


/*
 *        OTP format 
 *
 *        0               1               2               3
 *        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |             Vendor ID         |        Product    ID          |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |          LANG  ID             |   Max Power   | manufcator ID |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |   size      |   manufactor ID String                          |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                    manufactor ID String                       |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       | Product Str ID|    size       |        Product String         |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                       Product String                          |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       | serial str ID |    size       |        Serial number str      |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |         Serial number str                     |            
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

//Watch dog
#define WATCH_DOG_NOTIFY_PATTERN 0x00C60000

/* -------------------------------------------------------------
 *
 * Delay Time calculation methods.
 *
 * FULL-SPEED
 * 500 kb/s ~ 10 Mb/s
 *
 * HIGH-SPEED
 * 25 ~ 400 Mb/s
 *
 * Max TX packet may be 53 bytes (Send 7 descritors)
 * Max RX packet may be 4096 bytes (Receive for firmware download)
 *
 * -------------------------------------------------------------
 *
 *  So in Hign-speed.
 *
 *  Max time need is calculated as follow
 *
 *  each bytes need 
 *
 *  Transfer 25*1024*1024 = 26214400 bits /sec
 *                        = 3276800  bytes/sec
 *                        each byte needs 1/3276800  = 0.000000305175 sec
 *                                                   = 0.305 us
 *  60 bytes = 60*0.305 = 18.3 us
 *
 *----------------------------------------------------------------
 *  in Full-speed
 *
 *  Transfer 500*1024 = 512000 bits/sec
 *                    = 64000  bytes/sec
 *
 *  each byte needs 1/64000 = 0.000015625 sec
 *                          = 15 us
 *
 *  max tranfer bytes for tx= 64 bytes
 *  64*15 = 960 us
 *
 *  max receive bytes for rx=4096 (Firmware Download)
 *  4096*15 = 61440 us
 *
 */

#define TX_MAX_DELAY_USEC  960
#define RX_MAX_DELAY_USEC  61440

/* 
 * According to the USB 2.0 spec page 246, after a port is reset
 * or resumed, the USB system software is expected to provide a "recovery"
 * interval of 10 ms before the device attached to the port is expected to
 * data transfers.
 */
#define RESUME_FLUSH_DELAY_USEC 1000
#endif
