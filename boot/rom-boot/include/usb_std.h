#ifndef USB_STD_H
#define USB_STD_H


/*
 *   Token Packets
 *   
 *   Field | PID | ADDR | ENDPT | CRC5
 *   Bits  |  8  |  7   |   4   |   5
 *
 */

/******************** USB Protocol Definition *************************/
/* Standard Request Code (Table 9-4) */
#define USB_GET_STATUS          0
#define USB_CLEAR_FEATURE       1
#define USB_SET_FEATURE         3
#define USB_SET_ADDRESS         5
#define USB_GET_DESCRIPTOR      6
#define USB_SET_DESCRIPTOR      7
#define USB_GET_CONFIGURATION   8
#define USB_SET_CONFIGURATION   9
#define USB_GET_INTERFACE       10
#define USB_SET_INTERFACE       11
#define USB_SYNCH_FRAME         12

/* Descriptor Type (Table 9-5) */
#define USB_DESC_TYPE_DEVICE    1
#define USB_DESC_TYPE_CONFIG    2
#define USB_DESC_TYPE_STRING    3
#define USB_DESC_TYPE_INTERFACE 4
#define USB_DESC_TYPE_ENDPOINT  5

/* Stander Feature Selectors (Table 9-6) */

#define USB_ENDPOINT_HALT		    0	/* IN/OUT will STALL */
#define USB_DEVICE_REMOTE_WAKEUP	1	/* dev may initiate wakeup */
#define USB_DEVICE_TEST_MODE		2	/* (wired high speed only) */


/* Endpoint Attribute (Table 9-10) */
#define USB_EP_ATTR_CTRL        0
#define USB_EP_ATTR_ISOCH       1
#define USB_EP_ATTR_BULK        2
#define USB_EP_ATTR_INTRPT      3

/*********************** for USB 2.0 **********************************/
// Table 9-5. Descriptor Types
#define DT_DEVICE                       (0x01)
#define DT_CONFIGURATION                (0x02)
#define DT_STRING                       (0x03)
#define DT_INTERFACE                    (0x04)
#define DT_ENDPOINT                     (0x05)
#define DT_DEVICE_QUALIFIER             (0x06)
#define DT_OTHER_SPEED_CONFIGURATION    (0x07)
#define DT_INTERFACE_POWER              (0x08)

/**********************************************************************/
// Values for bmAttributes Field in USB_CONFIGURATION_DESCRIPTOR
#define USB_BUS_POWERED         0x80
#define USB_SELF_POWERED        0x40
#define USB_REMOTE_WAKEUP       0x20

#define cUSB_REQTYPE_DIR_POS    7
#define cUSB_REQTYPE_DIR_LEN    1
#define cUSB_REQTYPE_TYPE_POS   5
#define cUSB_REQTYPE_TYPE_LEN   2
#define cUSB_REQTYPE_RX_POS     0
#define cUSB_REQTYPE_RX_LEN     5

/* for USB State */
#define cUSB_DEFAULT_STATE      0
#define cUSB_ADDRESS_STATE      1
#define cUSB_CONFIG_STATE       2

/* for Data transfer direction */
#define bmUSB_HOST_DIR          7     /* Bit 7 */
#define cUSB_DIR_HOST_OUT       0
#define cUSB_DIR_HOST_IN        1



/* for Type */
#define cUSB_REQTYPE_STD        0
#define cUSB_REQTYPE_CLASS      1
#define cUSB_REQTYPE_VENDOR     2

/* for Recipient */
#define cUSB_REQTYPE_DEVICE     0
#define cUSB_REQTYPE_INTERFACE  1
#define cUSB_REQTYPE_ENDPOINT   2
#define cUSB_REQTYPE_OTHER      3

/* for Descriptor Type */
#define cUSB_DESTYPE_DEVICE     1
#define cUSB_DESTYPE_CONFIG     2
#define cUSB_DESTYPE_STRING     3
#define cUSB_DESTYPE_INTERFACE  4
#define cUSB_DESTYPE_ENDPOINT   5
#define cUSB_DESTYPE_END        cUSB_DESTYPE_ENDPOINT   // for range check

/* for Feature selector */
#define cUSB_FEATSEL_RMWAKEUP   0
#define cUSB_FEATSEL_EPHAL      1
#define cUSB_FEATSEL_END        cUSB_FEATSEL_EPHAL      // for range check

#define bmREQ_RECI              0
#define bmwREQ_RECI             5           // mMASKS(bmwREQ_RECI, bmREQ_RECI)
#define bmREQ_TYPE              5
#define bmwREQ_TYPE             2           // mMASKS(bmwREQ_TYPE, bmREQ_TYPE)
#define bmREQ_DIR               7
#define bmwREQ_DIR              1

/* other data need in usb spec */
#define USB_BCDUSB              0x0200      //USB 2.0

/******************** Max Packet Size Constraints *********************/
/* 5.5.3 Control Transfer Packet Size Constratints */
/*  Full Speed 8,16,32,64, High speed 64           */
#define CONTROL_MAX_PKT_SIZE       (64)    

/* 5.7.3 Interrupt Transfer Packet Size Constratints */
#define INTERRUPT_FS_MAX_PKT_SIZE    (64)
#define INTERRUPT_HS_MAX_PKT_SIZE   (1024)

/* 5.8.3 Bulk Transfer Packet Size Constratints */
#define BULK_FS_MAX_PKT_SIZE		 (64)
#define BULK_HS_MAX_PKT_SIZE	    (512)

/**********************************************************************/
//Data Structure for Descriptor

/* Table 9-2 */
struct usb_ctrlrequest {
	__u8 bRequestType;
	__u8 bRequest;
	__u16 wValue;
	__u16 wIndex;
	__u16 wLength;
} __attribute__ ((packed));

/* Table 9-8 */
struct usb_device_descriptor
{
	__u8	bLength;
	__u8	bDescriptorType;
	__u16	bcdUSB;
	__u8	bDeviceClass;
	__u8	bDeviceSubClass;
	__u8	bDeviceProtocol;
	__u8	bMaxPacketSize0;       /* Max Pkt Size for endpt 0 */
	__le16	idVendor;
	__le16  idProduct;	
	__le16	bcdDevice;
	__u8	iManufacturer;
	__u8	iProduct;
	__u8	iSerialNumber;
	__u8	bNumConfigurations;
} __attribute__ ((packed));

/* Table 9-9 */
/* USB_DT_DEVICE_QUALIFIER: Device Qualifier descriptor */
struct usb_qualifier_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__u8  bNumConfigurations;
	__u8  bRESERVED;
} __attribute__ ((packed));

/* Table 9-10 */
struct usb_configuration_descriptor
{
	__u8	bLength;
	__u8    bDescriptorType;
	__le16	wTotalLength;
	__u8	bNumInterfaces;
	__u8	bConfigurationValue;
	__u8	iConfiguration;
	__u8	bmAttributes;
	__u8	MaxPower;
} __attribute__ ((packed));


/* Table 9-12 */
/* USB_DT_INTERFACE: Interface descriptor */
struct usb_interface_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bInterfaceNumber;
	__u8  bAlternateSetting;
	__u8  bNumEndpoints;
	__u8  bInterfaceClass;
	__u8  bInterfaceSubClass;
	__u8  bInterfaceProtocol;
	__u8  iInterface;
} __attribute__ ((packed));

/* Table 9-13 */
/* USB_DT_ENDPOINT: Endpoint descriptor */
struct usb_endpoint_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bEndpointAddress;
	__u8  bmAttributes;
	__le16 wMaxPacketSize;
	__u8  bInterval;
} __attribute__ ((packed));


/* Table 9-15 */
/* USB_DT_STRING: String descriptor */
struct usb_string_descriptor0 {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 wData[1];		/* UTF-16LE encoded */
};// __attribute__ ((packed));

/* note that "string" zero is special, it holds language codes that
 * the device supports, not Unicode characters.
 */

/* Table 9-16 */
#define MAX_STR_SIZE 128
struct usb_string_descriptor 
{
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 wData[MAX_STR_SIZE];		/* UTF-16LE encoded */
} __attribute__ ((packed));
#endif
