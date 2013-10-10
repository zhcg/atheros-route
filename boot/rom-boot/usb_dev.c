#include <wasp_api.h>
#include <config.h>
#include "mem_use.h"
#include "usb_13320a_defs.h"
#include "usb_std.h"
#include "usb_config.h"
#include "fwd.h"
#include "dv_dbg.h"
#include <apb_map.h>
#include <ar7240_soc.h>
#include <asm/addrspace.h>

#define COLD_START     0
#define WARM_START     1
#define WATCHDOG_RESET 2

enum {
	OK,
	ERROR
};

enum {
	USB_HIGH_SPEED,
	USB_FULL_SPEED
};

__u32 firmware_dest_addr = 0;
__u32 firmware_curr_dest_addr = 0;
__u32 firmware_size = 0;

#define readb(addr) \
    ({ unsigned char __v = (*(volatile unsigned char *) (addr)); __v; })
#define readw(addr) \
    ({ unsigned short __v = (*(volatile unsigned short *) (addr)); __v; })
#define readl(addr) \
    ({ unsigned int __v = (*(volatile unsigned int *) (addr)); __v; })

#define writeb(b,addr) (void)((*(volatile unsigned char *) (addr)) = (b))
#define writew(b,addr) (void)((*(volatile unsigned short *) (addr)) = (b))
#define writel(b,addr) (void)((*(volatile unsigned int *) (addr)) = (b))

#define swap16(x)		\
    ((((x) & 0xff00) >> 8) |	\
     (((x) & 0x00ff) <<  8) )

#define cpu_to_le32(x)			\
    ((((x) & 0xff000000) >> 24) |	\
     (((x) & 0x00ff0000) >>  8) |	\
     (((x) & 0x0000ff00) <<  8) |	\
     (((x) & 0x000000ff) << 24))

#define swap32(x) cpu_to_le32(x)
#define TO_HW_ADDR(x) (((x) & 0x1FFFFFFF) )	// Let first 3 bits to 0, EX: Transfer 0xBDxxxxxx->0x1Dxxxxxx

//#define debug 0
#ifdef debug
#define usbdbg			A_PRINTF
#else
#define usbdbg(...)
#endif

void USB_endpoint_setup(__u8 endpt_num, __u8 dir, __u8 transfer_mode);

ENDPT0 endpt0;

USB_CONFIG usb_setting;

//Following data is little-endian mode
struct usb_device_descriptor device_des;
struct usb_qualifier_descriptor qualifier_des;

ENDPT_CONFIG endpt_settings[USB_ENDPT_NUM] = {
	/*  address   direction       type      */
	{1, cUSB_DIR_HOST_OUT, USB_EP_ATTR_BULK},
	{2, cUSB_DIR_HOST_IN, USB_EP_ATTR_BULK},
	{3, cUSB_DIR_HOST_IN, USB_EP_ATTR_BULK},
	{4, cUSB_DIR_HOST_OUT, USB_EP_ATTR_BULK},
	{5, cUSB_DIR_HOST_OUT, USB_EP_ATTR_BULK},
};

struct total_cfg_descriptor {
	struct usb_configuration_descriptor cfg_des;	//9
	struct usb_interface_descriptor if_des;	//9
	struct usb_endpoint_descriptor endpt_des[USB_ENDPT_NUM];	//7*USB_ENDPT_NUM
};

struct total_cfg_descriptor total_cfg_des;
struct total_cfg_descriptor other_total_cfg_des;

struct usb_string_descriptor0 string_descriptor0 = {
	0x04,			/* bLength(8) */
	0x03,			/* bUnicodeType(8) */
	{swap16(USB_LANG_ID)},	/* wLANGID(16): Language ID:1033 */
};

struct usb_string_descriptor manufacturer_str_des;
struct usb_string_descriptor product_str_des;
struct usb_string_descriptor serial_number_str_des;

__u8 manufacturer_string_des_lang0409[] =	//ATHEROS
{
	0x10, 0x03, 0x41, 0x00, 0x54, 0x00, 0x48, 0x00,
	0x45, 0x00, 0x52, 0x00, 0x4f, 0x00, 0x53, 0x00
};

__u8 product_string_des_lang0409[] =	//USB2.0 WLAN
{
	0x18, 0x03, 0x55, 0x00, 0x53, 0x00, 0x42, 0x00,
	0x32, 0x00, 0x2E, 0x00, 0x30, 0x00, 0x20, 0x00,
	0x57, 0x00, 0x4C, 0x00, 0x41, 0x00, 0x4E, 0x00
};

__u8 serial_number_string_des_lang0409[] =	//12345
{
	0x0C, 0x03, 0x31, 0x00, 0x32, 0x00, 0x33, 0x00,
	0x34, 0x00, 0x35, 0x00
};

/*
 *	Function list area
 */

void prepare_dtd(int rx_total_bytes, int tx_total_bytes);
void setup_endpt0_dtd(int rx_total_bytes, int tx_total_bytes, __u8 * data);

void init_mem()
{
	int index = 0;
	__s32 *ptr;

	for (index = 0, ptr = (__s32 *) DTD0_ADDR; index < (DTD_SIZE * DTD_PER_ENDPT) / 4; index++) {
		*ptr = 0;
		ptr++;
	}

	for (index = 0, ptr = (__s32 *) EP0_OUT_LIST_ADDR; index < (QH_SIZE * (USB_ENDPT_NUM + 1) * 2) / 4; index++) {
		*ptr = 0;
		ptr++;
	}
}

UDC udc;

/* Basic unit functions */

/* used for allocating one dtd in init stage */

void print_dtd(EP_DTD * ptr)
{
	A_PRINTF("\nDTD-> 0x%x\n", (__u32) ptr);
	A_PRINTF("next_dtd-> 0x%x\n", swap32(ptr->next_dtd));
	A_PRINTF("size_ioc_status 0x%x\n", swap32(ptr->size_ioc_status));
	A_PRINTF("BUF-> 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			swap32(ptr->buff[0]),
			swap32(ptr->buff[1]),
			swap32(ptr->buff[2]),
			swap32(ptr->buff[3]),
			swap32(ptr->buff[4]));
	A_PRINTF("next-> 0x%x\n", (__u32) ptr->next);
}

void prepare_dtd(int rx_total_bytes, int tx_total_bytes)
{
	//Fill in totoal byes in DTD
	endpt0.rx.dtd->next_dtd |= cpu_to_le32(CI13320A_DTD_TERMINATE);
	endpt0.rx.dtd->size_ioc_status = cpu_to_le32((rx_total_bytes << CI13320A_DTD_TOTALBYTESSHFIT) | CI13320A_DTD_IOC | CI13320A_DTD_ACTIVE);
	endpt0.rx.dtd->buff[0] = cpu_to_le32(TO_HW_ADDR(BUF0_BASE_ADDR));
	endpt0.rx.dtd->buff[1] = 0;

	endpt0.tx.dtd->next_dtd |= cpu_to_le32(CI13320A_DTD_TERMINATE);
	endpt0.tx.dtd->size_ioc_status = cpu_to_le32((tx_total_bytes << CI13320A_DTD_TOTALBYTESSHFIT) | CI13320A_DTD_IOC | CI13320A_DTD_ACTIVE);
	endpt0.tx.dtd->buff[0] = cpu_to_le32(TO_HW_ADDR(BUF1_BASE_ADDR));
	endpt0.tx.dtd->buff[1] = 0;

	//SETUP  QUEUE HEADER AGAIN
	endpt0.rx.qh->maxPacketLen = cpu_to_le32(CI13320A_QH_ZLENGTH | (CONTROL_MAX_PKT_SIZE << CI13320A_QH_SIZESHIFT) | CI13320A_QH_IOS);
	endpt0.rx.qh->size_ioc_status = 0;
	endpt0.rx.qh->curr_dtd = cpu_to_le32(TO_HW_ADDR(DTD0_ADDR));
	endpt0.rx.qh->next_dtd = cpu_to_le32(TO_HW_ADDR(DTD0_ADDR));

	endpt0.tx.qh->maxPacketLen = cpu_to_le32(CI13320A_QH_ZLENGTH | (CONTROL_MAX_PKT_SIZE << CI13320A_QH_SIZESHIFT) | CI13320A_QH_IOS);
	endpt0.tx.qh->size_ioc_status = 0;
	endpt0.tx.qh->curr_dtd = cpu_to_le32(TO_HW_ADDR(DTD1_ADDR));
	endpt0.tx.qh->next_dtd = cpu_to_le32(TO_HW_ADDR(DTD1_ADDR));
}

void do_endpt0_rx(int rx_total_bytes)
{
	int maxDelay = RX_MAX_DELAY_USEC;
	endpt0.rx.dtd->next_dtd |= cpu_to_le32(CI13320A_DTD_TERMINATE);
	endpt0.rx.dtd->size_ioc_status = cpu_to_le32((rx_total_bytes << CI13320A_DTD_TOTALBYTESSHFIT) | CI13320A_DTD_IOC | CI13320A_DTD_ACTIVE);
	endpt0.rx.dtd->buff[0] = cpu_to_le32(TO_HW_ADDR(BUF0_BASE_ADDR));
	endpt0.rx.dtd->buff[1] = 0;

	endpt0.rx.qh->maxPacketLen = cpu_to_le32(CI13320A_QH_ZLENGTH | (CONTROL_MAX_PKT_SIZE << CI13320A_QH_SIZESHIFT) | CI13320A_QH_IOS);
	endpt0.rx.qh->size_ioc_status = 0;
	endpt0.rx.qh->curr_dtd = cpu_to_le32(TO_HW_ADDR(DTD0_ADDR));
	endpt0.rx.qh->next_dtd = cpu_to_le32(TO_HW_ADDR(DTD0_ADDR));

	writel(CI13320A_USBENDPTPRIME_ENDPT0_RX, CI13320A_USB_ENDPTPRIME);

	while ((readl(CI13320A_USB_ENDPTCOMPLETE) & CI13320A_USBENDPTPRIME_ENDPT0_RX) == 0) {
		udelay(1);
		if ((maxDelay--) <= 0)
			break;
	}

	writel(CI13320A_USBENDPTPRIME_ENDPT0_RX, CI13320A_USB_ENDPTCOMPLETE);
}

void do_endpt0_tx(int tx_total_bytes, __u8 * data)
{
	int maxDelay = TX_MAX_DELAY_USEC;
	endpt0.rx.dtd->next_dtd |= cpu_to_le32(CI13320A_DTD_TERMINATE);
	//Fill the data into Endpt 0 Tx Buffer, setup qh of endpt 0, prime it!
	__u8 *dst = (__u8 *) BUF1_BASE_ADDR;
	int i;

	for (i = 0; i < tx_total_bytes; i++) {
		*dst = *data;
		data++;
		dst++;
	}

	endpt0.tx.dtd->next_dtd |= cpu_to_le32(CI13320A_DTD_TERMINATE);
	endpt0.tx.dtd->size_ioc_status = cpu_to_le32((tx_total_bytes << CI13320A_DTD_TOTALBYTESSHFIT) | CI13320A_DTD_IOC | CI13320A_DTD_ACTIVE);
	endpt0.tx.dtd->buff[0] = cpu_to_le32(TO_HW_ADDR(BUF1_BASE_ADDR));
	endpt0.tx.dtd->buff[1] = 0;

	endpt0.tx.qh->maxPacketLen = cpu_to_le32(CI13320A_QH_ZLENGTH | (CONTROL_MAX_PKT_SIZE << CI13320A_QH_SIZESHIFT) | CI13320A_QH_IOS);
	endpt0.tx.qh->size_ioc_status = 0;
	endpt0.tx.qh->curr_dtd = cpu_to_le32(TO_HW_ADDR(DTD1_ADDR));
	endpt0.tx.qh->next_dtd = cpu_to_le32(TO_HW_ADDR(DTD1_ADDR));

	writel(CI13320A_USBENDPTPRIME_ENDPT0_TX, CI13320A_USB_ENDPTPRIME);

	i = 0;
	while ((readl(CI13320A_USB_ENDPTCOMPLETE) &
		CI13320A_USBENDPTPRIME_ENDPT0_TX) == 0) {
		udelay(1);
		if ((maxDelay--) <= 0)
			break;
	}

	writel(CI13320A_USBENDPTPRIME_ENDPT0_TX, CI13320A_USB_ENDPTCOMPLETE);
}

void setup_endpt0_dtd(int rx_total_bytes, int tx_total_bytes, __u8 * data)
{
	//Fill the data into Endpt 0 Tx Buffer, setup qh of endpt 0, prime it!
	__u8 *dst = (__u8 *) BUF1_BASE_ADDR;
	int i;

	for (i = 0; i < tx_total_bytes; i++) {
		*dst = *data;
		data++;
		dst++;
	}

	prepare_dtd(rx_total_bytes, tx_total_bytes);

#if 0
	{
		int i = 0;

		A_UART_PUTS("\n<-----Re Init Queue Head and DTD\n");
		A_UART_PUTS("\nNow print Queue Head of Endpt 0\n");
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->maxPacketLen));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->curr_dtd));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->next_dtd));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->size_ioc_status));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->buff[0]));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->buff[1]));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->buff[2]));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->buff[3]));
		usbdbg("0x%x\n", swap32(endpt0.rx.qh->buff[4]));
		for (i = 0; i < 8; i++)
			A_UART_PUTU8(endpt0.rx.qh->setup_buff[i]);

		A_UART_PUTS("\nNow print DTD\n");
		print_dtd(endpt0.rx.dtd);
	}
#endif

	//	writel( CI13320A_USBENDPTPRIME_ENDPT0_TX |CI13320A_USBENDPTPRIME_ENDPT0_RX,CI13320A_USB_ENDPTPRIME);
}

void USB_init_endpt()
{
	usbdbg("Enter USB_Endpt_Init...\n");

	init_mem();

	//Endpt, Queue Head, DTD init

	//Set ENDPT QH OUT/IN Addr and it's DTD Addr
	endpt0.rx.qh = (EP_QH *) EP0_OUT_LIST_ADDR;
	endpt0.tx.qh = (EP_QH *) EP0_IN_LIST_ADDR;
	endpt0.rx.dtd = (EP_DTD *) DTD0_ADDR;
	endpt0.tx.dtd = (EP_DTD *) DTD1_ADDR;

	//Configure ENDPOINTLISTADDR
	writel(TO_HW_ADDR((__u32) (endpt0.rx.qh)), CI13320A_USB_ENDPOINTLISTADDR);

	usbdbg("Leave USB_Endpt_Init...\n");
}

void USB_get_status(struct usb_ctrlrequest *ctrl)
{
	usbdbg("Enter USB_get_status...\n");
	__u16 status = 0;
	__u8 endptId;

	switch (ctrl->wIndex) {
	case 0:		//Device
		status = 0;
		break;
	default:
		//Find out the status according to which Endpt
		endptId = ctrl->wIndex & 0xFF;
		status = swap16(udc.ep[endptId].halt);
		break;
	}

	A_USB_DO_ENDPT0_TX(2, (__u8 *) & status);
	A_USB_DO_ENDPT0_RX(PAGE_4K);
	usbdbg("Leave USB_get_status...\n");
}

void USB_watchdog_reset_notify()
{
	/*
	 * ------------------------------------------------------------------------------------
	 * | EP0 OUT | EP0 IN | EP1 OUT | EP1 IN | EP2 OUT | EP2 IN | EP3 OUT | EP3 IN | ......
	 * ------------------------------------------------------------------------------------
	 * |<- 64  ->|
	 */
	int maxDelay = TX_MAX_DELAY_USEC;
	__u32 data = WATCH_DOG_NOTIFY_PATTERN;

	EP_QH *pQH = (EP_QH *) (EP0_OUT_LIST_ADDR + QH_SIZE * 7);
	EP_DTD *pDTD = (EP_DTD *) DTD1_ADDR;
	__u32 *dst = (__u32 *) BUF1_BASE_ADDR;	//Borrow Buffer1/DTD1 from EP0 temporarily

	dst[0] = data;

	pDTD->next_dtd |= cpu_to_le32(TO_HW_ADDR(CI13320A_DTD_TERMINATE));
	pDTD->size_ioc_status = cpu_to_le32((sizeof(data) << CI13320A_DTD_TOTALBYTESSHFIT) | CI13320A_DTD_ACTIVE);
	pDTD->buff[0] = cpu_to_le32(TO_HW_ADDR(BUF1_BASE_ADDR));
	pDTD->buff[1] = 0;

	pQH->maxPacketLen = cpu_to_le32(CI13320A_QH_ZLENGTH | (BULK_FS_MAX_PKT_SIZE << CI13320A_QH_SIZESHIFT) | CI13320A_QH_IOS);
	pQH->size_ioc_status = 0;
	pQH->curr_dtd = cpu_to_le32(TO_HW_ADDR(DTD1_ADDR));
	pQH->next_dtd = cpu_to_le32(TO_HW_ADDR(DTD1_ADDR));

	A_UART_PUTS("Send WatchDog Notify through EndPt 3...\n");

	writel(CI13320A_USBENDPTPRIME_ENDPT3_TX, CI13320A_USB_ENDPTPRIME);

	while ((readl(CI13320A_USB_ENDPTCOMPLETE) &
		CI13320A_USBENDPTPRIME_ENDPT3_TX) == 0) {
		udelay(1);
		if ((maxDelay--) <= 0)
			break;
	}

	writel(CI13320A_USBENDPTPRIME_ENDPT3_TX, CI13320A_USB_ENDPTCOMPLETE);
	usbdbg("Leave USB_watchdog_reset_notify...\n");

}

void USB_set_address(struct usb_ctrlrequest *ctrl)
{
	//set DEVICE ADDR into Register 0x154h
	int i;
	__u32 wValue = ctrl->wValue;

	usbdbg("Enter USB_set_address...\n");

	A_USB_DO_ENDPT0_TX(0, 0);

	do {
		writel((wValue << CI13320A_USBDEVICE_ADDR_SHIFT), CI13320A_USB_DEVICEADDR);
		usbdbg("Write new EndPoint Address USB_set_address... 0x%x\n", ctrl->wValue);
	} while (readl(CI13320A_USB_DEVICEADDR) != (wValue << CI13320A_USBDEVICE_ADDR_SHIFT));

	//According to data sheet, An Endpoint should be enabled only after it has been configured.
	//So wait until got address
	for (i = 0; i < USB_ENDPT_NUM; i++) {
		USB_endpoint_setup(endpt_settings[i].address,
				   endpt_settings[i].direction,
				   endpt_settings[i].type);
	}

	udc.dev_status = 1;

	A_UART_PUTS("Address and Endpoints are configured\n");
	A_UART_PUTU32(ctrl->wValue);

	if (udc.watchdog_notify == 1) {
		udc.watchdog_notify = 0;
		A_USB_WATCHDOG_RESET_NOTIFY();
	}
	usbdbg("Leave USB_set_address...\n");
}

void USB_get_qualifier(struct usb_ctrlrequest *ctrl)
{
	int size = sizeof(struct usb_qualifier_descriptor);
	usbdbg("Enter USB_get_qualifier...\n");

	if (ctrl->wLength < size)
		size = ctrl->wLength;

	A_USB_DO_ENDPT0_TX(size, (__u8 *) & qualifier_des);
	A_USB_DO_ENDPT0_RX(PAGE_4K);
	usbdbg("Leave USB_get_qualifier...\n");
}

void USB_get_other_speed_configuration(struct usb_ctrlrequest *ctrl)
{
	__u8 *src_ptr = NULL;
	int total_bytes;

	usbdbg("Enter USB_get_other_speed_configuration...\n");
	usbdbg("ctrl->wLength-> 0x%x\n", ctrl->wLength);
	switch (ctrl->wLength) {
	case sizeof(struct usb_configuration_descriptor):
		src_ptr = (__u8 *) & other_total_cfg_des.cfg_des;
		total_bytes = sizeof(struct usb_configuration_descriptor);
		break;
	default:
		src_ptr = (__u8 *) & other_total_cfg_des;
		total_bytes = sizeof(struct total_cfg_descriptor);
		break;
	}

	A_USB_DO_ENDPT0_TX(total_bytes, src_ptr);
	A_USB_DO_ENDPT0_RX(PAGE_4K);
	usbdbg("Leave USB_get_other_speed_configuration...\n");

}

int get_current_speed()
{
	if (readl(CI13320A_USB_PORTSC0) & CI13320A_USB_PORTSC0_HSP) {
		return USB_HIGH_SPEED;
	} else {
		return USB_FULL_SPEED;
	}
}

void init_device_des()
{
	device_des.bLength = sizeof(struct usb_device_descriptor);
	device_des.bDescriptorType = DT_DEVICE;
	device_des.bcdUSB = swap16(USB_BCDUSB);
	device_des.bDeviceClass = USB_DEVICE_CLASS;
	device_des.bDeviceSubClass = USB_DEVICE_SUBCLASS;
	device_des.bDeviceProtocol = USB_DEVICE_PROTOCOL;
	device_des.bMaxPacketSize0 = CONTROL_MAX_PKT_SIZE;
	device_des.idVendor = swap16(usb_setting.idVendor);
	device_des.idProduct = swap16(usb_setting.idProduct);
	device_des.bcdDevice = swap16(USB_BCD_DEVICE);
	device_des.iManufacturer = usb_setting.iManufacturer;
	device_des.iProduct = usb_setting.iProduct;
	device_des.iSerialNumber = usb_setting.iSerialNumber;
	device_des.bNumConfigurations = 0x1;
}

void init_qualifier_des()
{
	qualifier_des.bLength = sizeof(struct usb_qualifier_descriptor);
	qualifier_des.bDescriptorType = DT_DEVICE_QUALIFIER;
	qualifier_des.bcdUSB = swap16(USB_BCDUSB);;
	qualifier_des.bDeviceClass = USB_DEVICE_CLASS;
	qualifier_des.bDeviceSubClass = USB_DEVICE_SUBCLASS;
	qualifier_des.bDeviceProtocol = USB_DEVICE_PROTOCOL;
	qualifier_des.bMaxPacketSize0 = CONTROL_MAX_PKT_SIZE;
	qualifier_des.bNumConfigurations = 1;
	qualifier_des.bRESERVED = 0;
}

void init_configuration_des()
{
	total_cfg_des.cfg_des.bLength = sizeof(struct usb_configuration_descriptor);
	total_cfg_des.cfg_des.bDescriptorType = DT_CONFIGURATION;

	total_cfg_des.cfg_des.wTotalLength = swap16(sizeof(struct total_cfg_descriptor));
	total_cfg_des.cfg_des.bNumInterfaces = 0x01;
	total_cfg_des.cfg_des.bConfigurationValue = 0x01;
	total_cfg_des.cfg_des.iConfiguration = 0x00;
	total_cfg_des.cfg_des.bmAttributes = 0x80;
	total_cfg_des.cfg_des.MaxPower = usb_setting.bMaxPower;

	total_cfg_des.if_des.bLength = sizeof(struct usb_interface_descriptor);
	total_cfg_des.if_des.bDescriptorType = DT_INTERFACE;
	total_cfg_des.if_des.bInterfaceNumber = 0;
	total_cfg_des.if_des.bAlternateSetting = 0;
	total_cfg_des.if_des.bNumEndpoints = USB_ENDPT_NUM;
	total_cfg_des.if_des.bInterfaceClass = 0xFF;
	total_cfg_des.if_des.bInterfaceSubClass = 0;
	total_cfg_des.if_des.bInterfaceProtocol = 0;
	total_cfg_des.if_des.iInterface = 0;
}

void init_other_speed_cfg_des()
{
	int i;
	__u8 *src_ptr, *dst_ptr;

	//Make a copy from total_cfg_des
	src_ptr = (__u8 *) & (total_cfg_des);
	dst_ptr = (__u8 *) & (other_total_cfg_des);

	for (i = 0; i < sizeof(struct total_cfg_descriptor); i++) {
		*dst_ptr = *src_ptr;
		dst_ptr++;
		src_ptr++;
	}

	other_total_cfg_des.cfg_des.bDescriptorType = DT_OTHER_SPEED_CONFIGURATION;
	for (i = 0; i < USB_ENDPT_NUM; i++) {
		if (get_current_speed() == USB_HIGH_SPEED) {
			other_total_cfg_des.endpt_des[i].wMaxPacketSize =
			    swap16(BULK_FS_MAX_PKT_SIZE);
		} else {
			other_total_cfg_des.endpt_des[i].wMaxPacketSize =
			    swap16(BULK_HS_MAX_PKT_SIZE);
		}
	}
}

void init_endpt_des()
{
	int i = 0;
	for (; i < USB_ENDPT_NUM; i++) {
		total_cfg_des.endpt_des[i].bLength = sizeof(struct usb_endpoint_descriptor);
		total_cfg_des.endpt_des[i].bDescriptorType = DT_ENDPOINT;
		total_cfg_des.endpt_des[i].bEndpointAddress = (endpt_settings[i].address) | (endpt_settings[i].direction == cUSB_DIR_HOST_OUT ? 0 : 0x80);
		total_cfg_des.endpt_des[i].bmAttributes = endpt_settings[i].type;

		if (get_current_speed() == USB_HIGH_SPEED) {
			A_PRINTF("HS\n");
			total_cfg_des.endpt_des[i].wMaxPacketSize = swap16(BULK_HS_MAX_PKT_SIZE);
		} else {
			A_PRINTF("FS\n");
			total_cfg_des.endpt_des[i].wMaxPacketSize = swap16(BULK_FS_MAX_PKT_SIZE);
		}

		total_cfg_des.endpt_des[i].bInterval = 0;
	}
}

void *get_configuration(struct usb_ctrlrequest *ctrl, int *size)
{
	void *dst_ptr = NULL;

	//Work around for bug ExtraView ID 65785,set Endpt DES/Other Speed DES again
	//Becuase at this time the Speed query should be OK, no more speed detected error
	init_endpt_des();
	init_other_speed_cfg_des();

	switch (ctrl->wLength) {
	case sizeof(struct usb_configuration_descriptor):
		dst_ptr = &total_cfg_des.cfg_des;
		*size = sizeof(struct usb_configuration_descriptor);
		break;
	default:
		dst_ptr = &total_cfg_des;
		*size = sizeof(struct total_cfg_descriptor);
		break;
	}

	return dst_ptr;
}

void USB_set_descriptor(struct usb_ctrlrequest *ctrl)
{

}

void USB_get_descriptor(struct usb_ctrlrequest *ctrl)
{
	__u8 *src_ptr = NULL;
	int total_bytes = 0;

	usbdbg("Enter USB_get_descriptor...\n");

	switch (ctrl->wValue >> 8) {
	case DT_DEVICE:
		usbdbg("->DT_DEVICE\n");
		total_bytes = sizeof(device_des);
		src_ptr = (__u8 *) & device_des;
		break;
	case DT_CONFIGURATION:
		usbdbg("->DT_CONFIGURATION\n");
		src_ptr = (__u8 *) get_configuration(ctrl, &total_bytes);
		break;
	case DT_STRING:
		usbdbg("->DT_STRING\n");
		if ((ctrl->wValue & 0xFF) == 0) {
			usbdbg("-->index 0x0\n");
			total_bytes = sizeof(struct usb_string_descriptor0);
			src_ptr = (__u8 *) & string_descriptor0;
		} else if ((ctrl->wValue & 0xFF) ==
			   usb_setting.iManufacturer) {
			if (udc.read_from_otp == 0) {
				usbdbg("-->index 16, langID 0x0409\n");
				total_bytes = sizeof(manufacturer_string_des_lang0409);
				src_ptr = (__u8 *) & manufacturer_string_des_lang0409;
			} else {
				usbdbg("-->index 16\n");
				total_bytes = manufacturer_str_des.bLength;
				src_ptr = (__u8 *) & manufacturer_str_des;
			}
		} else if ((ctrl->wValue & 0xFF) == usb_setting.iProduct) {
			if (udc.read_from_otp == 0) {
				usbdbg("-->index 32,langID 0x0409\n");
				total_bytes = sizeof(product_string_des_lang0409);
				src_ptr = (__u8 *) & product_string_des_lang0409;
			} else {
				usbdbg("-->index 32\n");
				total_bytes = product_str_des.bLength;
				src_ptr = (__u8 *) & product_str_des;
			}
		} else if ((ctrl->wValue & 0xFF) ==
			   usb_setting.iSerialNumber) {
			if (udc.read_from_otp == 0) {
				usbdbg("-->index 48, lang 0x0409\n");
				total_bytes = sizeof (serial_number_string_des_lang0409);
				src_ptr = (__u8 *) & serial_number_string_des_lang0409;
			} else {
				usbdbg("-->index 16\n");
				total_bytes = serial_number_str_des.bLength;
				src_ptr = (__u8 *) & serial_number_str_des;
			}
		} else {
			usbdbg("->Could not deal!\n");
		}
		break;
	case DT_INTERFACE:
		usbdbg("->DT_INTERFACE\n");
		break;
	case DT_ENDPOINT:
		usbdbg("->DT_ENDPOINT\n");
		break;
	case DT_DEVICE_QUALIFIER:
		usbdbg("->DT_QUALIFIER\n");
		USB_get_qualifier(ctrl);
		return;
		break;
	case DT_OTHER_SPEED_CONFIGURATION:
		usbdbg("->DT_SPEED_CONFIGURATION\n");
		USB_get_other_speed_configuration(ctrl);
		return;
		break;
	case DT_INTERFACE_POWER:
		usbdbg("->DT_INTERFACE_POWER\n");
		break;
	default:
		usbdbg("!!!!Should not enter here!\n");
		break;
	}

	A_USB_DO_ENDPT0_TX(total_bytes, src_ptr);
	A_USB_DO_ENDPT0_RX(0);

	usbdbg("Leave USB_get_descriptor...\n");
}

void USB_get_configuration()
{
	__u8 data = udc.configuration;
	usbdbg("Enter USB_get_configuration...\n");

	A_USB_DO_ENDPT0_TX(1, &data);
	A_USB_DO_ENDPT0_RX(PAGE_4K);

	usbdbg("Leave USB_get_configuration...\n");
}

void USB_set_configuration(struct usb_ctrlrequest *ctrl)
{

	usbdbg("Enter USB_set_configuration...\n");

	udc.configuration = (__u32) (ctrl->wValue & 0xFF);

	A_USB_DO_ENDPT0_TX(0, 0);
	usbdbg("Leave USB_set_configuration...\n");
	A_UART_PUTS("USB_set_configuration...\n");
	A_UART_PUTU32(udc.configuration);

}

#define TEST_J                  0x02
#define TEST_K                  0x04
#define TEST_SE0_NAK            0x08
#define TEST_PKY                0x10

__u8 TestPatn0[] = { TEST_J, TEST_K, TEST_SE0_NAK };

/* Test packet for Test Mode : TEST_PACKET. USB 2.0 Specification section 7.1.20 */
__u8 TestPatn1[] = {
	/* Sync */
	/* DATA 0 PID */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
	0xEE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xBF, 0xDF,
	0xEF, 0xF7, 0xFB, 0xFD, 0xFC, 0x7E, 0xBF, 0xDF,
	0xEF, 0xF7, 0xFB, 0xFD, 0x7E
};

void USB_set_feature(struct usb_ctrlrequest *ctrl)
{
	//Check wIndex
	int endptId = ctrl->wIndex & 0xFF;
	int total_bytes = 0;
	void *data = NULL;
	__u32 value = 0;

	switch (ctrl->wValue) {
	case USB_ENDPOINT_HALT:
		udc.ep[endptId].halt = 1;
		total_bytes = 0;
		break;
	case USB_DEVICE_REMOTE_WAKEUP:
		break;
	case USB_DEVICE_TEST_MODE:
		switch (ctrl->wIndex >> 8)	//Chap 9 Table 9-7
		{
		case 0x1:	// Test_J
			usbdbg("TEST_J\n");
			data = &TestPatn0[0x1];
			total_bytes = 1;
			value = TEST_MODE_J_STATE;
			break;
		case 0x2:	// Test_K
			usbdbg("TEST_K\n");
			data = &TestPatn0[0x2];
			total_bytes = 1;
			value = TEST_MODE_K_STATE;
			break;
		case 0x3:	// TEST_SE0_NAK
			usbdbg("TEST_SE0_NAK\n");
			data = &TestPatn0[0x3];
			total_bytes = 1;
			value = TEST_MODE_SE0_NAK;
			break;
		case 0x4:	// Test_PACKET
			usbdbg("TEST_PACKET\n");
			value = TEST_MODE_TEST_PACKET;
			data = NULL;
			total_bytes = 0;
			break;
		case 0x5:	// Test_Force_Enable
		default:
			break;
		}
		break;
	default:
		break;
	}

	if (ctrl->wValue == USB_DEVICE_TEST_MODE) {
		A_USB_DO_ENDPT0_TX(0, 0);

		writel(readl(CI13320A_USB_PORTSC0) | (value << CI13320A_USB_PORTSC0_TEST_MODE), CI13320A_USB_PORTSC0);

		if (value == TEST_MODE_TEST_PACKET) {
			total_bytes = sizeof(TestPatn1);
			data = &TestPatn1;
			A_USB_DO_ENDPT0_TX(total_bytes, data);
		}
	} else {
		A_USB_DO_ENDPT0_TX(total_bytes, data);
	}
}

void USB_clear_feature(struct usb_ctrlrequest *ctrl)
{
	//Check wIndex
	int endptId = ctrl->wIndex & 0xFF;

	switch (ctrl->wValue) {
	case USB_ENDPOINT_HALT:
		udc.ep[endptId].halt = 0;
		break;
	case USB_DEVICE_REMOTE_WAKEUP:
		break;
	case USB_DEVICE_TEST_MODE:
		break;
	default:
		break;
	}

	A_USB_DO_ENDPT0_TX(0, NULL);
	A_USB_DO_ENDPT0_RX(PAGE_4K);
}

void USB_set_interface(struct usb_ctrlrequest *ctrl)
{
	A_USB_DO_ENDPT0_TX(0, NULL);
}

void USB_get_interface(struct usb_ctrlrequest *ctrl)
{
	__u8 data = 0;

	A_USB_DO_ENDPT0_TX(1, &data);
	A_USB_DO_ENDPT0_RX(PAGE_4K);
}

void USB_endpoint_setup(__u8 endpt_num, __u8 dir, __u8 transfer_mode)
{
	__u32 endpt_reg_address = CI13320A_USB_ENDPTCTRL0 + 4 * endpt_num;
	__u32 endpt_config = 0;
	__u8 transfer_mode_shift = 0;
	__u32 data_toggle = 0;

	usbdbg("enter USB_endpoint_setup\n");

	endpt_config = readl(endpt_reg_address);
	endpt_config |= ((dir == cUSB_DIR_HOST_IN) ? CI13320A_USB_ENDPTCTRL_TX_ENABLE : CI13320A_USB_ENDPTCTRL_RX_ENABLE);
	transfer_mode_shift = ((dir == cUSB_DIR_HOST_IN) ?  CI13320A_USB_ENDPTCTRL_TX_TRANSFER_MODE_SHIFT : CI13320A_USB_ENDPTCTRL_RX_TRANSFER_MODE_SHIFT);
	data_toggle = (dir == cUSB_DIR_HOST_IN) ?  CI13320A_USB_ENDPTCTRL_TX_DATA_TOGGLE_RESET : CI13320A_USB_ENDPTCTRL_RX_DATA_TOGGLE_RESET;
	endpt_config |= (transfer_mode << transfer_mode_shift) | data_toggle;

	switch (transfer_mode) {
	case USB_EP_ATTR_CTRL:
		usbdbg("USB_EP_ATTR_CTRL\n");
		break;
	case USB_EP_ATTR_ISOCH:
		usbdbg("USB_EP_ATTR_ISOCH\n");
		break;
	case USB_EP_ATTR_BULK:
		usbdbg("USB_EP_ATTR_BULK\n");
		break;
	case USB_EP_ATTR_INTRPT:
		usbdbg("USB_EP_ATTR_INTRPT\n");
		break;
	default:
		usbdbg("should not enter here!\n");
	}
	writel(endpt_config, endpt_reg_address);

	usbdbg("leave USB_endpoint_setup\n");
}

__u32
checksum(__u32 addr, __u32 size)
{
	__u32 *data = (__u32 *) addr;
	__u32 checksum = 0;
	int i;

	for (i = 0; i < size; i += 4) {
		checksum = checksum ^ *data;
		data++;
	}

	return checksum;
}

__u8 verify_firmware_checksum(__u32 addr, __u32 size)
{
	A_UART_PUTS("addr\n");
	A_UART_PUTU32(addr);
	A_UART_PUTS("size\n");
	A_UART_PUTU32(size);

	return (checksum(addr, size) == 0) ? OK : ERROR;
}

void USB_jump_to_fw(int jump_addr, int firmware_size)
{
	A_UART_PUTS("Jump now to addr->");
	A_UART_PUTU32(jump_addr);

	if (jump_addr != 0) {
		call_fw(HIF_USB, jump_addr);
	}
}

void USB_Handle_Vendor_Request(struct usb_ctrlrequest *ctrl)
{
	__u8 ret = 1;

	if ((ctrl->bRequest == FIRMWARE1_DOWNLOAD) || (ctrl->bRequest == FIRMWARE2_DOWNLOAD)) {
		__u32 total_txbytes, total_rxbytes;
		int i = 0;
		__u8 *src_ptr = (__u8 *) (BUF0_BASE_ADDR);
		__u8 *dst_ptr = 0;

		if (firmware_dest_addr == 0) {
			firmware_dest_addr = (__u32) (ctrl->wValue << 16) + (__u32) ctrl->wIndex;
			firmware_curr_dest_addr = firmware_dest_addr;
			A_PRINTF("SET FIRMWARE_DOWNLOAD addr 0x%x\n", firmware_dest_addr);
		}
#if 0
		A_UART_PUTS("now start to receive data at addr, length\n");
		A_UART_PUTU32(firmware_curr_dest_addr);
		A_UART_PUTU32(ctrl->wLength);
#else
		A_UART_PUTS(".");
#endif

		total_txbytes = PAGE_4K;
		A_USB_DO_ENDPT0_RX(total_txbytes);

		total_rxbytes = total_txbytes - (__u32) (swap32(endpt0.rx.qh->size_ioc_status) >> CI13320A_DTD_TOTALBYTESSHFIT);

		dst_ptr = (__u8 *) firmware_curr_dest_addr;

		//A_PRINTF("%p %p %u\n", dst_ptr, src_ptr, total_rxbytes);
		for (i = 0; i < total_rxbytes; i++) {
			*dst_ptr = *src_ptr;
			dst_ptr++;
			src_ptr++;
		}

		firmware_curr_dest_addr += total_rxbytes;
		firmware_size += total_rxbytes;
		A_USB_DO_ENDPT0_TX(0, 0);
	} else if ((ctrl->bRequest == FIRMWARE1_DOWNLOAD_COMP)
		   || (ctrl->bRequest == FIRMWARE2_DOWNLOAD_COMP)) {
		__u32 jump_addr;
		__u32 jump_firmware_size;

		A_PRINTF("FIRMWARE_DOWNLOAD_COMP 0x%x %u\n", firmware_dest_addr, firmware_size);
		//Verify checksum
		ret = verify_firmware_checksum(firmware_dest_addr, firmware_size);

		//report status of download result
		A_PRINTF("Checksum value is : 0x%x\n", ret);

		//Send result to host, if it's 0 means firmware checksum verified OK
		A_USB_DO_ENDPT0_TX(1, &ret);
		A_USB_DO_ENDPT0_RX(0);

		jump_addr = (__u32) (ctrl->wValue << 16) + (__u32) ctrl->wIndex;
		jump_firmware_size = firmware_size;
		//Reset the firmware dest addr
		firmware_dest_addr = 0;
		firmware_curr_dest_addr = 0;
		firmware_size = 0;
		if (ret == OK) {
			A_PRINTF("Jumping to 0x%x\n", jump_addr);
			A_USB_JUMP_TO_FW(jump_addr, jump_firmware_size);
			A_PRINTF ("* * * * * * * * Return back from f/w * * * * * * * *\n");
		} else {
			usbdbg("Checksum Error!\n");
		}
	}
}

void USB_Handle_Setup()
{
	int i;

	struct usb_ctrlrequest ctrl;
	__u8 *ptr = (__u8 *) & ctrl;

	//copy setup to local memory
	for (i = 0; i < 8; i++) {
		*(ptr + i) = endpt0.rx.qh->setup_buff[i];
	}

	ctrl.wValue = swap16(ctrl.wValue);
	ctrl.wIndex = swap16(ctrl.wIndex);
	ctrl.wLength = swap16(ctrl.wLength);

	//Clear the ENDPTSTEUPSTAT
	writel(CI13320A_USBSETUPSTAT_ENDPT0, CI13320A_USB_ENDPTSETUPSTAT);

	while ((readl(CI13320A_USB_ENDPTSETUPSTAT) & CI13320A_USBSETUPSTAT_ENDPT0) != 0) {
		usbdbg("Wait for clearing CI13320A_USB_ENDPTSETUPSTAT!\n");
	}

	if ((ctrl.bRequestType == SETUP_VENDOR_REQUEST_DL) || (ctrl.bRequestType == SETUP_VENDOR_REQUEST_CMP)) {
		usbdbg("Vendor Request\n");
		A_USB_HANDLE_VENDOR_REQUEST(&ctrl);
		return;
	}

	usbdbg("Setup\n");
	switch (ctrl.bRequest) {
	case USB_GET_STATUS:
		usbdbg("USB_GET_STATUS\n");
		A_USB_GET_STATUS(&ctrl);
		break;
	case USB_CLEAR_FEATURE:
		usbdbg("USB_CLEAR_FEATURE\n");
		A_USB_CLEAR_FEATURE(&ctrl);
		break;
	case USB_SET_FEATURE:
		usbdbg("USB_SET_FEATURE\n");
		A_USB_SET_FEATURE(&ctrl);
		break;
	case USB_SET_ADDRESS:
		usbdbg("USB_SET_ADDRESS\n");
		A_USB_SET_ADDRESS(&ctrl);
		break;
	case USB_GET_DESCRIPTOR:
		usbdbg("USB_GET_DESCRIPTOR\n");
		A_USB_GET_DESCRIPTOR(&ctrl);
		break;
	case USB_SET_DESCRIPTOR:
		usbdbg("USB_SET_DESCRIPTOR\n");
		A_USB_SET_DESCRIPTOR(&ctrl);
		break;
	case USB_GET_CONFIGURATION:
		usbdbg("USB_GET_CONFIGURATION\n");
		A_USB_GET_CONFIGURATION();
		break;
	case USB_SET_CONFIGURATION:
		usbdbg("USB_SET_CONFIGURATION\n");
		A_USB_SET_CONFIGURATION(&ctrl);
		break;
	case USB_GET_INTERFACE:
		usbdbg("USB_GET_INTERFACE\n");
		A_USB_GET_INTERFACE(&ctrl);
		break;
	case USB_SET_INTERFACE:
		usbdbg("USB_SET_INTERFACE\n");
		A_USB_SET_INTERFACE(&ctrl);
		break;
	default:
		usbdbg("USB_HandleSetup: should no enter here!\n");
		A_UART_PUTU8(ctrl.bRequest);
		usbdbg("\n");
	}
}

void USB_Handle_Error(__u32 usb_status)
{
	A_PRINTF("%s: 0x%x\n", __func__, usb_status);

}

void USB_Handle_Interrupt()
{
	__u32 endpt_setup_stat = readl(CI13320A_USB_ENDPTSETUPSTAT);
	__u32 usb_status = readl(CI13320A_USB_USBSTS);
	writel(usb_status, CI13320A_USB_USBSTS);

	if (usb_status & CI13320A_USBSTS_ERROR_INTERRUPT) {
		A_USB_HANDLE_ERROR(usb_status);
	}
	//Check ENDPTSETUPSTATUS for handling SETUP
	if (endpt_setup_stat & CI13320A_USBSETUPSTAT_ENDPT0) {
		A_USB_HANDLE_SETUP();
	}
}

void USB_init_stat()
{
	__u32 value;
	int maxDelay = RESUME_FLUSH_DELAY_USEC;

	writel(CI13320A_USBSTS_RESETRECV | CI13320A_USBSTS_DCSUSPEND, CI13320A_USB_USBSTS);

	value = readl(CI13320A_USB_ENDPTSETUPSTAT);
	writel(value, CI13320A_USB_ENDPTSETUPSTAT);

	value = readl(CI13320A_USB_ENDPTCOMPLETE);
	writel(value, CI13320A_USB_ENDPTCOMPLETE);

	while (readl(CI13320A_USB_ENDPTPRIME) != 0) {
		udelay(1);
		if ((maxDelay--) <= 0)
			break;
	}

	writel(0xFFFFFFFF, CI13320A_USB_ENDPTFLUSH);

	//Wait until all bits of CI13320A_USB_ENDPTFLUSH become 0
	maxDelay = RESUME_FLUSH_DELAY_USEC;
	while (readl(CI13320A_USB_ENDPTFLUSH) != 0) {
		udelay(1);
		if ((maxDelay--) <= 0)
			break;
	}
}

int make_str_des(struct usb_string_descriptor *str_des, __u32 size,
		 __u32 descriptorType, __u8 * data)
{
	int i = 0;

	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	if (size > MAX_STR_SIZE / 2) {
		DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
		return ERROR;
	}

	str_des->bLength = size * 2 + 2;	//size*2->unicode, then plus 1 bytes for size, 1 byte for descriptor type
	str_des->bDescriptorType = descriptorType;

	for (i = 0; i < size; i++) {
		str_des->wData[i] = (__le16) swap16(*data);
		data++;
	}

	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	return OK;
}

int read_otp_data(__u8 * otp_ptr)
{
	__u32 size;
	__u32 index = 0;
	int ret = ERROR;

	usb_setting.idVendor = (__u16) (otp_ptr[0] << 8) + ((__u16) otp_ptr[1]);
	usb_setting.idProduct = (__u16) (otp_ptr[2] << 8) + ((__u16) otp_ptr[3]);
	string_descriptor0.wData[0] = swap16((__u16) (otp_ptr[4] << 8) + ((__u16) otp_ptr[5]));
	usb_setting.bMaxPower = otp_ptr[6];

	index = 7;
	usb_setting.iManufacturer = otp_ptr[index];	//7
	size = otp_ptr[++index];	//8 size
	ret = make_str_des(&manufacturer_str_des, size, 0x3, &otp_ptr[index + 1]);
	if (ret == ERROR)
		return ERROR;

	index += size;
	usb_setting.iProduct = otp_ptr[++index];
	size = otp_ptr[++index];
	ret = make_str_des(&product_str_des, size, 0x3, &otp_ptr[index + 1]);
	if (ret == ERROR)
		return ERROR;

	index += size;
	if (otp_ptr[++index] != 0)	//Serial Number string may be Empty, so check this value
	{
		usb_setting.iSerialNumber = otp_ptr[index];
		size = otp_ptr[++index];
		ret = make_str_des(&serial_number_str_des, size, 0x3, &otp_ptr[index + 1]);
		if (ret == ERROR)
			return ERROR;
	} else {
		usb_setting.iSerialNumber = 0;
	}

	return OK;
}

void USB_set_default_value()
{
	usb_setting.idVendor = USB_ID_VENDOR;
	usb_setting.idProduct = USB_ID_PRODUCT;
	usb_setting.iManufacturer = USB_MANUFACTURER;
	usb_setting.iProduct = USB_PRODUCT;
	usb_setting.iSerialNumber = USB_SERIAL_NUMBER;
	usb_setting.bMaxPower = USB_MAX_POWER;
	usb_setting.bUSBPhyBias = USB_PHY_BIAS;
}

void USB_config_phy_bias()
{
	__u8 custom_usb_phy_bias = USB_PHY_BIAS;
#if !CONFIG_WASP_EMU
	__u32 value;
#endif

	//Config USB Phy Bias
	/*    1) turn off wlan reset and wait eepbusy bit clean before you try to access any wlan related register
	 *
	 *    2) turn off wlan rtc reset and wait wlan rtc is ON state
	 *
	 *    3) Write usb bias value to address 0x18116C88 bit [25:22]
	 *
	 *    The Hardware default value is 3, and it seems working fine with odin. And we need some information
	 *    from analog team to see if the bit 21 need to clean if non default value is used.
	 */

#if 0
	/* De-assert WLAN RTC reset    write  0x1 to 0x18107040 bit0 (RTC_SYNC_RESET_L) */
	writel(readl(REG_WLAN_RTC) | RTC_SYNC_RESET_L, REG_WLAN_RTC);

	/* Polling 0x18107044 (RTC_SYNC_STATUS) wait for bit 1(RTC_SYNC_STATUS_ON_STATE) is 0x1 */
	while ((readl(REG_WLAN_RTC_SYNC_STATUS) & RTC_SYNC_STATUS_ON_STATE) == 0) ;
#endif

	/*
	 * USB PHY CTRL2 Register 0x18116C88
	 * bit [25:22] tx_man_cal , bit [21] tx_cal_sel, bit [20] tx_cal_en
	 *
	 * 1. If art calibration data is available
	 *    -> write tx_cal_sel and tx_cal_en to 0
	 *       and tx_man_cal with value located in otp ART calibration data
	 *
	 * 2. If no ART data and ATE calibration data is available
	 *    -> write tx_cal_sel and tx_cal_en to 0
	 *    and tx_man_cal with value located in otp static region
	 *
	 * 3. If no any calibration data located in otp
	 *    -> write tx_cal_sel and tx_cal_en to 0
	 *    and use tx_man_cal default value 0x3
	 *
	 */

	custom_usb_phy_bias = A_OTP_READ_USB_BIAS();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	DV_DBG_RECORD_DATA(custom_usb_phy_bias);

	// The correct value of Phy Bias should be 0~0xF
	if (custom_usb_phy_bias <= 0xF) {
		usb_setting.bUSBPhyBias = custom_usb_phy_bias;
	} else {
		usb_setting.bUSBPhyBias = USB_PHY_BIAS;
	}
#if !CONFIG_WASP_EMU
	value = readl(USB_PHY_CTRL2) & USB_PHY_CTRL2_TX_MAN_CAL_MASK;	//clear bit [25:22]
	value = value & ~(USB_PHY_CTRL2_TX_CAL_SEL | USB_PHY_CTRL2_TX_CAL_EN);	//write tx_cal_sel and tx_cal_en to 0
	value |= usb_setting.bUSBPhyBias << USB_PHY_CTRL2_TX_MAN_CAL_SHIFT;
	writel(value, USB_PHY_CTRL2);
#endif
	A_PRINTF("bUSBPhyBias=0x%x\n", usb_setting.bUSBPhyBias);
	DV_DBG_RECORD_DATA(usb_setting.bUSBPhyBias);
}

void read_usb_config()
{
	__u8 *custom_usb_info = NULL;
	int ret = ERROR;

	//Read data from OTP

	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	custom_usb_info = A_OTP_READ_USB();
	if (custom_usb_info) {
		/* A_UART_PUTS("==> USB info from OTP <==\n\r"); */
		ret = A_USB_READ_OTP_DATA((__u8 *) custom_usb_info);
		if (ret == ERROR) {
			udc.read_from_otp = 0;
			USB_set_default_value();
			DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
		} else {
			udc.read_from_otp = 1;
			DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
		}
	} else {
		udc.read_from_otp = 0;
		USB_set_default_value();
		DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
		/* A_UART_PUTS("==> USB info by default <==\n\r"); */ ;
	}

	A_PRINTF("VID=0x%x,PID=0x%x\n", usb_setting.idVendor,
		 usb_setting.idProduct);
	A_PRINTF("iManufacturer=0x%x,iProduct=0x%x,iSerialNumber=0x%x\n",
		 usb_setting.iManufacturer, usb_setting.iProduct, usb_setting.iSerialNumber);
	A_PRINTF("bMaxPower=0x%x\n", usb_setting.bMaxPower);
	DV_DBG_RECORD_DATA(usb_setting.idVendor);
	DV_DBG_RECORD_DATA(usb_setting.idProduct);
	DV_DBG_RECORD_DATA(usb_setting.iManufacturer);
	DV_DBG_RECORD_DATA(usb_setting.iProduct);
	DV_DBG_RECORD_DATA(usb_setting.iSerialNumber);
	DV_DBG_RECORD_DATA(usb_setting.bMaxPower);
}

void USB_setup_descriptor()
{
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	init_device_des();
	init_configuration_des();
	init_endpt_des();
	init_qualifier_des();
	init_other_speed_cfg_des();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
}

__u8 get_start_type()
{
	__u32 *ptr = (__u32 *) SRAM_START_TYPE_ADDR;
	usbdbg("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			*ptr, *(ptr + 1), *(ptr + 2),
			*(ptr + 3), readl(BOOT_TYPE_REGISTER));

	if ((*ptr == 0x2a415448) &&	/*  *ATHEROS+WASP*  */
	    (*(ptr + 1) == 0x45524f53) &&
	    (*(ptr + 2) == 0x2B484F52) && (*(ptr + 3) == 0x4E45542A)
	    ) {
		A_UART_PUTS("-> WARM_START\n");
		*ptr = 0;
		*(ptr + 1) = 0;
		*(ptr + 2) = 0;
		*(ptr + 3) = 0;
		return WARM_START;
	} else if (readl(BOOT_TYPE_REGISTER) & BOOT_TYPE_REGISTER_WATCHDOG_RESET) {
		A_UART_PUTS("-> WATCHDOG_RESET\n");
		return WATCHDOG_RESET;
	} else {
		A_UART_PUTS("-> COLD_START\n");
		return COLD_START;
	}
}

void USB_Init_cold_start()
{
	// USB PHY Bias should be configured before stop the USB controller/PHY RESET
	// And it should only be configured in COLD_START

	A_USB_CONFIG_PHY_BIAS();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	//Init Reset Register
	//DV SIM modify to correct RESET value
#if 0
	writel((readl(RESET_REGISTER) & ~(RESET_USB_HOST_RESET | RESET_USB_PHY_ARESET | RESET_USB_PHY_RESET)) | RESET_SUSPEND_OVERRIDE, RESET_REGISTER);
#else
	ar7240_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_USB_PHY_SUSPEND_OVERRIDE_MASK);

	ar7240_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_ARESET_MASK);

	udelay(250);

	ar7240_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_RESET_MASK);

	udelay(1);

	ar7240_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_HOST_RESET_MASK);
#endif


	//Data sheet 8.2.1 Bus Reset, Hardware Reset (RESET set 1 in USBCMD) will casue
	//device detach, so only have hardware reset when in COLD_START
	usbdbg("USB Reset\n");
	writel(readl(CI13320A_USB_USBCMD) | CI13320A_USBCMD_RESET, CI13320A_USB_USBCMD);
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	//Wait until reset to be complete
	while (readl(CI13320A_USB_USBCMD) & CI13320A_USBCMD_RESET) {
		//usbdbg("Wait reset to be complete\n");
	}
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	udc.watchdog_notify = 0;
	udc.dev_status = 0;
	//Set the USB Device Mode
	writel(readl(CI13320A_USB_USBMODE) | CI13320A_USBMODE_DEVICE, CI13320A_USB_USBMODE);
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	A_USB_INIT_ENDPT();

#if 0
	/*
	 * Dont enable interrupts here.
	 * The device has a couple of interrupts pending and the line
	 * goes high. Eventually, when linux boots and enables irq
	 * (in cp0 status) usb driver is not ready yet and there is
	 * nobody to handle and/or ack the interrupts resulting in
	 * cpu hang.
	 *	-Varada
	 */
	//Enable Interrupt
	writel((readl(CI13320A_USB_USBINTR) | CI13320A_USBINTR_INTR
		| CI13320A_USBINTR_ERROR_INTR
		| CI13320A_USBINTR_PORT_CHANGE_INTR
		| CI13320A_USBINTR_RESETRECV)
	       , CI13320A_USB_USBINTR);
#else
	A_PRINTF("%s: Not enabling Interrupt.\n", __func__);
#endif

	//Set to Run Stat
	writel(readl(CI13320A_USB_USBCMD) | CI13320A_USBCMD_RUN, CI13320A_USB_USBCMD);
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	A_UART_PUTS("RUN\n");

	//Waiting FOR USB RESET to finish before checking for setup trans
	while (CI13320A_USBSTS_RESETRECV != (readl(CI13320A_USB_USBSTS) & CI13320A_USBSTS_RESETRECV)) {
		//usbdbg("W RESET\n");
	}
	A_UART_PUTS("Default State\n");
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	//Clear RESETRECV
	writel(CI13320A_USBSTS_RESETRECV, CI13320A_USB_USBSTS);
}

void USB_Init_watchdog_reset()
{
	//Note: We don't know if USB status is fine after Watch-Dog reset
	//      so reinit it again!
	A_USB_INIT_STAT();
	A_USB_INIT_ENDPT();

	//if it's WATCHDOG_RESET, send packets to notify host by endpt 3 until get address
	A_USB_WATCHDOG_RESET_NOTIFY();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
}

void USB_Init_warm_start()
{
	//Should config the ENDPTLISTADDR again ,because firmware may change this to DDR address apace
	A_USB_INIT_STAT();
	A_USB_INIT_ENDPT();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
}

void do_USB_reset()
{
	//In data sheet P66. In order to ensure that the devie is not in an
	//attached state before initiating a device controller reset, all
	//primed endpoints should be flushed and the USBCMD Run/Stop bit
	//should be set to 0

	//Flush all the data
	writel(0xFFFFFFFF, CI13320A_USB_ENDPTFLUSH);

	//Wait until all bits of CI13320A_USB_ENDPTFLUSH become 0
	while (readl(CI13320A_USB_ENDPTFLUSH) != 0) {
		;
	}

	//Set to Stop State
	writel(readl(CI13320A_USB_USBCMD) & ~CI13320A_USBCMD_RUN, CI13320A_USB_USBCMD);

	usbdbg("USB Reset\n");
	writel(readl(CI13320A_USB_USBCMD) | CI13320A_USBCMD_RESET, CI13320A_USB_USBCMD);
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	//Wait until reset to be complete
	while (readl(CI13320A_USB_USBCMD) & CI13320A_USBCMD_RESET) {
		//usbdbg("Wait reset to be complete\n");
	}
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	udc.watchdog_notify = 0;
	udc.dev_status = 0;
	//Set the USB Device Mode
	writel(readl(CI13320A_USB_USBMODE) | CI13320A_USBMODE_DEVICE, CI13320A_USB_USBMODE);
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	A_USB_INIT_ENDPT();

#if 0
	/*
	 * Dont enable interrupts here.
	 * The device has a couple of interrupts pending and the line
	 * goes high. Eventually, when linux boots and enables irq
	 * (in cp0 status) usb driver is not ready yet and there is
	 * nobody to handle and/or ack the interrupts resulting in
	 * cpu hang.
	 *	-Varada
	 */
	//Enable Interrupt
	writel((readl(CI13320A_USB_USBINTR) | CI13320A_USBINTR_INTR
		| CI13320A_USBINTR_ERROR_INTR
		| CI13320A_USBINTR_PORT_CHANGE_INTR
		| CI13320A_USBINTR_RESETRECV)
	       , CI13320A_USB_USBINTR);
#else
	A_PRINTF("%s: Not enabling Interrupt.\n", __func__);
#endif

	//Set to Run Stat
	writel(readl(CI13320A_USB_USBCMD) | CI13320A_USBCMD_RUN, CI13320A_USB_USBCMD);
	DV_DBG_RECORD_LOCATION(USB_DEV_C); // Location Pointer
	A_UART_PUTS("RUN\n");

	//Waiting FOR USB RESET to finish before checking for setup trans
	while (CI13320A_USBSTS_RESETRECV != (readl(CI13320A_USB_USBSTS) & CI13320A_USBSTS_RESETRECV)) {
		//usbdbg("W RESET\n");
	}
	A_UART_PUTS("Default State\n");
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	//Clear RESETRECV
	writel(CI13320A_USBSTS_RESETRECV, CI13320A_USB_USBSTS);

}

void USB_Init_watchdog_reset_with_resetUSB()
{
	do_USB_reset();

	//Because we reset the USB, so we only can send packets until endpoints
	//are configured
	udc.watchdog_notify = 1;
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
}

void USB_Init_warm_start_with_resetUSB()
{
	do_USB_reset();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
}

void USB_Init_by_starttype()
{
	udc.start_type = get_start_type();
	if (udc.start_type == COLD_START) {
		A_USB_INIT_COLD_START();
	} else if (udc.start_type == WATCHDOG_RESET) {
		A_USB_INIT_WATCHDOG_RESET();
	} else if (udc.start_type == WARM_START) {
		A_USB_INIT_WARM_START();
	}
}

void USB_Init_device()
{
	usbdbg("enter USB_Init_device...\n");

	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	A_USB_READ_USB_CONFIG();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	A_USB_INIT_BY_STARTTYPE();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer
	A_USB_SETUP_DESCRIPTOR();
	DV_DBG_RECORD_LOCATION(USB_DEV_C);	// Location Pointer

	usbdbg("Leave USB_Init_device...\n");
}

void usb_module_install(struct usb_api *api)
{
	api->_usb_init = USB_Init_device;
	api->_usb_init_stat = USB_init_stat;
	api->_usb_init_endpt = USB_init_endpt;
	api->_usb_setup_descriptor = USB_setup_descriptor;
	api->_usb_handle_interrupt = USB_Handle_Interrupt;
	api->_usb_handle_vendor_request = USB_Handle_Vendor_Request;
	api->_usb_handle_error = USB_Handle_Error;
	api->_usb_handle_setup = USB_Handle_Setup;
	api->_usb_get_status = USB_get_status;
	api->_usb_clear_feature = USB_clear_feature;
	api->_usb_set_feature = USB_set_feature;
	api->_usb_set_address = USB_set_address;
	api->_usb_get_descriptor = USB_get_descriptor;
	api->_usb_set_descriptor = USB_set_descriptor;
	api->_usb_get_configuration = USB_get_configuration;
	api->_usb_set_configuration = USB_set_configuration;
	api->_usb_get_interface = USB_get_interface;
	api->_usb_set_interface = USB_set_interface;
	api->_usb_read_usb_config = read_usb_config;
	api->_usb_read_otp_data = read_otp_data;
	api->_usb_do_endpt0_rx = do_endpt0_rx;
	api->_usb_do_endpt0_tx = do_endpt0_tx;
	api->_usb_jump_to_fw = USB_jump_to_fw;
	api->_usb_watchdog_reset_notify = USB_watchdog_reset_notify;
	api->_usb_init_by_starttype = USB_Init_by_starttype;
	api->_usb_config_phy_bias = USB_config_phy_bias;
	api->_usb_init_cold_start = USB_Init_cold_start;
	api->_usb_init_warm_start = USB_Init_warm_start;
	api->_usb_init_watchdog_reset = USB_Init_watchdog_reset;
}
