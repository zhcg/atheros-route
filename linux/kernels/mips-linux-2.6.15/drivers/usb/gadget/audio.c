#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/utsname.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <linux/compiler.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>

#include <linux/usb_ch9.h>
#include <linux/usb_gadget.h>
#include <linux/proc_fs.h>

#include "gadget_chips.h"
#include "audio.h"

/*-------------------------------------------------------------------------*/

#define DRIVER_VERSION		"Atheros Jan 2010"

static const char shortname [] = "Audio";
static const char longname [] = "Gadget Audio Mixer/Speaker";

static const char audio_gadget[] = "Audio Speaker";

/*-------------------------------------------------------------------------*/
/******************************For DEBUG************************************/

static off_t count_set_config;
static off_t count_get_config_desc;
static off_t count_get_dev_desc;
static off_t req_set_interface;
static off_t req_get_interface;
static off_t audio_set_alt_count;
static unsigned config_num;
static unsigned char g_bRequest;
static off_t totalSetups;
static unsigned g_value, g_length, g_index;
static unsigned g_RequestType;
static off_t out_ep_complete_count;
static off_t audio_intfreq_complete_count;
static off_t count_audio_playback;
static off_t totalDatCnt;
static off_t out_ep_created;
static int list_element_no;


#ifdef BUFDUMP_ENABLED_WQ
static char *audio_dump; 
#endif
static unsigned long bufOff;
/******************************For DEBUG************************************/

#define OUT_EP_MAX_PACKET_SIZE  192
static int req_buf_size = OUT_EP_MAX_PACKET_SIZE;
module_param(req_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(req_buf_size, "ISO OUT endpoint request buffer size");

static int req_count = 256;
module_param(req_count, int, S_IRUGO);
MODULE_PARM_DESC(req_count, "ISO OUT endpoint request count");


#define MAX_AUDIO_CHAN (4)
#define BUF_SIZE_FACTOR 4
#define AUDIO_FRAME_SIZE ((OUT_EP_MAX_PACKET_SIZE)*8)
static int audio_buf_size = (AUDIO_FRAME_SIZE*BUF_SIZE_FACTOR);/*768x48*/
module_param(audio_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(audio_buf_size, "Audio buffer size");
#define BUFFER_SIZE 5

static void audio_suspend (struct usb_gadget *);
static void audio_resume (struct usb_gadget *);
static void audio_complete(struct usb_ep *, struct usb_request *);
extern void wlan_get_tsf(off_t *);
extern int wlan_aow_tx(char *data, int datalen, int channel, off_t tsf);

/*
 * driver assumes self-powered hardware, and
 * has no way for users to trigger remote wakeup.
 *
 * this version autoconfigures as much as possible,
 * which is reasonable for most "bulk-only" drivers.
 */
static const char *EP_IN_NAME;		/* source */
static const char *EP_OUT_NAME;		/* sink */
#define USB_BUFSIZ	512

#define WLAN_AOW_ENEBLED 1

struct audio_buf {
        u8 *buf;
        int actual;
        struct list_head list;
};


//#define PLAYBACK_QUEUE_STATIC 1
#define AUDIO_INTF_REQ_NUM 100
struct audio_dev {
	spinlock_t		lock;
	struct usb_gadget	*gadget;
	struct usb_request	*req, *confreq;		/* for control responses */

	atomic_t 		contreq_status;
	struct usb_ep		*out_ep, *status_ep;
        struct usb_endpoint_descriptor  *out, *status;

	/*For playback*/
	struct audio_buf *copy_buf;
	struct list_head play_queue;
	#ifdef PLAYBACK_QUEUE_STATIC
	struct list_head free_queue;
	#endif
    /*Queue for maintaining request buffers.*/
	struct list_head req_queue;


	u8			config;
	u8			interface;
	u8			altSetting;
	u8			curAltSetting;
	unsigned 		urb_created;
	unsigned		suspended;

	/*Work queue for pumping the data*/
	struct work_struct playback_work;

};

static void audio_reset_config (struct audio_dev *);

/*-------------------------------------------------------------------------*/

#define xprintk(d,level,fmt,args...) \
	dev_printk(level , &(d)->gadget->dev , fmt , ## args)

#ifdef DEBUG
#define DBG(dev,fmt,args...) \
	xprintk(dev , KERN_DEBUG , fmt , ## args)
#else
#define DBG(dev,fmt,args...) \
	do { } while (0)
#endif /* DEBUG */

#ifdef VERBOSE
#define VDBG	DBG
#else
#define VDBG(dev,fmt,args...) \
	do { } while (0)
#endif /* VERBOSE */

#define ERROR(dev,fmt,args...) \
	xprintk(dev , KERN_ERR , fmt , ## args)
#define WARN(dev,fmt,args...) \
	xprintk(dev , KERN_WARNING , fmt , ## args)
#define INFO(dev,fmt,args...) \
	xprintk(dev , KERN_INFO , fmt , ## args)

/*-------------------------------------------------------------------------*/


/*Values needs to be rechecked */

#define DEV_CONFIG_VALUE 1
/*
 * We have two interfaces- AudioControl and AudioStreaming
 * TODO: only supcard playback currently
 */
#define F_AUDIO_AC_INTERFACE    0
#define F_AUDIO_AS_INTERFACE    1
#define F_AUDIO_NUM_INTERFACES  1

static struct usb_device_descriptor
device_desc = {
	.bLength =		    USB_DT_DEVICE_SIZE,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		    __constant_cpu_to_le16 (0x0200),
	.bDeviceClass =		0x0,
	.bDeviceSubClass =	0x0,
	.bDeviceProtocol =	0x0,
	.bMaxPacketSize0 =  0x40,
	.idVendor =		    __constant_cpu_to_le16 (0),
	.idProduct =		__constant_cpu_to_le16 (0),
	.iManufacturer =	STRING_MANUFACTURER,
	.iProduct =		    STRING_PRODUCT,
	.iSerialNumber =	STRING_SERIAL,
	.bNumConfigurations =	1,
};

static struct usb_config_descriptor
audio_config = {
	.bLength =		    USB_DT_CONFIG_SIZE,
	.bDescriptorType =	USB_DT_CONFIG,

	/* compute wTotalLength on the fly */
	.bNumInterfaces =	2,
	.bConfigurationValue =	DEV_CONFIG_VALUE,
	.iConfiguration =	STRING_AUDIO,
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		50,	/* self-powered */
};

static struct usb_otg_descriptor
otg_descriptor = {
	.bLength =		    sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,
	.bmAttributes =		USB_OTG_SRP,
};

static const struct usb_interface_descriptor
audio_ac_intf = {
	.bLength =		    USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting =0,

	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_AUDIO_SUBCLASS_CONTROL,
	.bInterfaceProtocol = 	0x0,
	.iInterface =		0x0,
};



#define UAC_DT_AC_HEADER_LENGTH UAC_DT_AC_HEADER_SIZE(F_AUDIO_NUM_INTERFACES)
/* 1 input terminal, 1 output terminal and 1 feature unit */
#define UAC_DT_TOTAL_LENGTH (UAC_DT_AC_HEADER_LENGTH + \
UAC_DT_INPUT_TERMINAL_SIZE \
       + UAC_DT_OUTPUT_TERMINAL_SIZE + UAC_DT_FEATURE_UNIT_SIZE(0))

static struct usb_ac_interface_desc 
ac_interface_desc = {
	.bLength = 		UAC_DT_AC_HEADER_LENGTH,
    .bDescriptorType =      USB_DT_CS_INTERFACE,
    .bDescriptorSubtype =   UAC_HEADER,
    .bcdADC =               __constant_cpu_to_le16(0x0100),
    .wTotalLength =         __constant_cpu_to_le16(UAC_DT_TOTAL_LENGTH),
    .bInCollection =        F_AUDIO_NUM_INTERFACES,
    .baInterfaceNr = {
            [0] =           F_AUDIO_AS_INTERFACE,
	}
};

#define INPUT_TERMINAL_ID       1
#define OUTPUT_TERMINAL_ID      2
#define UAC_DT_INPUT_TERMINAL_SIZE  12
static struct uac_input_terminal_descriptor input_terminal_desc = {
    .bLength =              UAC_DT_INPUT_TERMINAL_SIZE,
    .bDescriptorType =      USB_DT_CS_INTERFACE,
    .bDescriptorSubtype =   UAC_INPUT_TERMINAL,
    .bTerminalID =          INPUT_TERMINAL_ID,
    .wTerminalType =        __constant_cpu_to_le16(UAC_TERMINAL_STREAMING),
    .bAssocTerminal =       OUTPUT_TERMINAL_ID, 
 	.bNrChannels = 			0x8,
    .wChannelConfig =       __constant_cpu_to_le16(0x063F),
	.iChannelNames = 	    0x0,
	.iTerminal = 		    0x0,
};

#ifdef FEATURE_UNIT_SUPPORTED
DECLARE_UAC_FEATURE_UNIT_DESCRIPTOR(0);

#define FEATURE_UNIT_ID         13
static struct uac_feature_unit_descriptor_0 feature_unit_desc = {
    .bLength =              UAC_DT_FEATURE_UNIT_SIZE(0),
    .bDescriptorType =      USB_DT_CS_INTERFACE,
    .bDescriptorSubtype =   UAC_FEATURE_UNIT,
    .bUnitID =              FEATURE_UNIT_ID,
    .bSourceID =            INPUT_TERMINAL_ID,
    .bControlSize =         2,
    .bmaControls[0] =       (UAC_FU_MUTE | UAC_FU_VOLUME),
};
#endif

static struct uac_output_terminal_descriptor output_terminal_desc = {
    .bLength =              UAC_DT_OUTPUT_TERMINAL_SIZE,
    .bDescriptorType =      USB_DT_CS_INTERFACE,
    .bDescriptorSubtype =   UAC_OUTPUT_TERMINAL,
    .bTerminalID =          OUTPUT_TERMINAL_ID,
    .wTerminalType =        __constant_cpu_to_le16(UAC_OUTPUT_TERMINAL_SPEAKER),
    #ifdef FEATURE_UNIT_SUPPORTED
    .bAssocTerminal =       FEATURE_UNIT_ID,
    .bSourceID =            FEATURE_UNIT_ID,
    #else
    .bAssocTerminal =       INPUT_TERMINAL_ID,
    .bSourceID =            INPUT_TERMINAL_ID,
    #endif
    .iTerminal =            0x0,
};

static const struct usb_interface_descriptor
audio_streaming_intf_alt_0_desc = {
	.bLength =		        USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	    USB_DT_INTERFACE,
	.bInterfaceNumber = 	1,
	.bAlternateSetting = 	0,
	.bNumEndpoints =	    0,
	.bInterfaceClass =	    USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_AUDIO_SUBCLASS_STREAMING,
	.bInterfaceProtocol = 	0x0,
	.iInterface =		    0,
};
static const struct usb_interface_descriptor
audio_streaming_intf_alt_1_desc = {
	.bLength =		        USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	    USB_DT_INTERFACE,
	.bInterfaceNumber = 	1,
	.bAlternateSetting = 	1,
	.bNumEndpoints =	    1,
	.bInterfaceClass =	    USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_AUDIO_SUBCLASS_STREAMING,
	.bInterfaceProtocol = 	0x0,
	.iInterface =		    0,
};

static const struct usb_interface_descriptor
audio_streaming_intf_alt_2_desc = {
	.bLength =		        USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	    USB_DT_INTERFACE,
	.bInterfaceNumber = 	1,
	.bAlternateSetting = 	2,
	.bNumEndpoints =	    1,
	.bInterfaceClass =	    USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_AUDIO_SUBCLASS_STREAMING,
	.bInterfaceProtocol = 	0x0,
	.iInterface =		    0,
};


static const struct usb_interface_descriptor
audio_streaming_intf_alt_3_desc = {
	.bLength =		        USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	    USB_DT_INTERFACE,
	.bInterfaceNumber = 	1,
	.bAlternateSetting = 	3,
	.bNumEndpoints =	    1,
	.bInterfaceClass =	    USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_AUDIO_SUBCLASS_STREAMING,
	.bInterfaceProtocol = 	0x0,
	.iInterface =		    0,
};

static const struct usb_interface_descriptor
audio_streaming_intf_alt_4_desc = {
	.bLength =		        USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	    USB_DT_INTERFACE,
	.bInterfaceNumber = 	1,
	.bAlternateSetting = 	4,
	.bNumEndpoints =	    1,
	.bInterfaceClass =	    USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_AUDIO_SUBCLASS_STREAMING,
	.bInterfaceProtocol = 	0x0,
	.iInterface =		    0,
};
#define UAC_DT_AS_HEADER_SIZE		7
static struct usb_as_interface_desc
as_interface_desc = {
	.bLength = 		        UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType = 	    USB_DT_CS_INTERFACE,		
	.bDescriptorSubType = 	AS_GENERAL,

	.bTerminalLink =	    INPUT_TERMINAL_ID,
	.bDelay = 		        1,
	.wFormatTag = 		    __constant_cpu_to_le16(FORMAT_TYPE_PCM),
};


DECLARE_UAC_FORMAT_TYPE_I_DISCRETE_DESC(1);
#define UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(n)	(8 + (n * 3))

static struct uac_as_format_desc_1
as_format_alt_1_desc = {
	.bLength = 		        UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType = 	    USB_DT_CS_INTERFACE,
	.bDescriptorSubType = 	FORMAT_TYPE,

	.bFormatType = 		    UAC_FORMAT_TYPE_I,
	.bNrChannels = 		    2,
	.bSubFrameSize = 	    2,
	.bBitResolution = 	    0x10,
	.bSamFreqType = 	    1,
	.tSamFreq = 		    {
	                        	[0][0]	= 0x80,
	                        	[0][1]	= 0xBB,
                        	}
};

static struct uac_as_format_desc_1
as_format_alt_2_desc = {
	.bLength = 		        UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType = 	    USB_DT_CS_INTERFACE,
	.bDescriptorSubType = 	FORMAT_TYPE,

	.bFormatType = 		    UAC_FORMAT_TYPE_I,
	.bNrChannels = 		    4,
	.bSubFrameSize = 	    2,
	.bBitResolution = 	    0x10,
	.bSamFreqType = 	    1,
	.tSamFreq = 		    {
	                        	[0][0]	= 0x80,
	                        	[0][1]	= 0xBB,
	                        }
};

static struct uac_as_format_desc_1
as_format_alt_3_desc = {
	.bLength = 		UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType = 	USB_DT_CS_INTERFACE,
	.bDescriptorSubType = 	FORMAT_TYPE,

	.bFormatType = 		UAC_FORMAT_TYPE_I,
	.bNrChannels = 		6,
	.bSubFrameSize = 	2,
	.bBitResolution = 	0x10,
	.bSamFreqType = 	1,
	.tSamFreq = 		{
		[0][0]	= 0x80,
		[0][1]	= 0xBB,
	}
};

static struct uac_as_format_desc_1
as_format_alt_4_desc = {
	.bLength = 		UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType = 	USB_DT_CS_INTERFACE,
	.bDescriptorSubType = 	FORMAT_TYPE,

	.bFormatType = 		UAC_FORMAT_TYPE_I,
	.bNrChannels = 		8,
	.bSubFrameSize = 	2,
	.bBitResolution = 	0x10,
	.bSamFreqType = 	1,
	.tSamFreq = 		{
		[0][0]	= 0x80,
		[0][1]	= 0xBB,
	}
};
/* One full speed isocronous endpoints; their use is config-dependent */

static struct usb_endpoint_descriptor
fs_out_ep_alt_1_desc = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
	.wMaxPacketSize =   __constant_cpu_to_le16((1*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};

static struct usb_endpoint_descriptor
fs_out_ep_alt_2_desc = {
	.bLength =			USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize =   __constant_cpu_to_le16((2*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};

static struct usb_endpoint_descriptor
fs_out_ep_alt_3_desc = {
	.bLength =			USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
  	.wMaxPacketSize =  	__constant_cpu_to_le16((3*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};

static struct usb_endpoint_descriptor
fs_out_ep_alt_4_desc = {
	.bLength =			USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize =  	__constant_cpu_to_le16((4*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};
/* Class-specific AS ISO OUT Endpoint Descriptor */
//static struct uac_iso_endpoint_descriptor as_iso_out_desc __initdata = {
static struct uac_iso_endpoint_descriptor as_iso_out_desc = {
        .bLength =              UAC_ISO_ENDPOINT_DESC_SIZE,
        .bDescriptorType =      USB_DT_CS_ENDPOINT,
        .bDescriptorSubtype =   UAC_EP_GENERAL,
        .bmAttributes =         0,
        .bLockDelayUnits =      0,
        .wLockDelay =           __constant_cpu_to_le16(0),
};

#ifdef STATUS_INTERRUPT_EP_ENABLED
#define LOG2_STATUS_INTERVAL_MSEC	5	/* 1 << 5 == 32 msec */
#define STATUS_BYTECOUNT		16	/* 8 byte header + data */
static struct usb_endpoint_descriptor
hs_status_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16 (STATUS_BYTECOUNT),
	.bInterval =		LOG2_STATUS_INTERVAL_MSEC + 4,
};


static struct usb_endpoint_descriptor
fs_status_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16 (STATUS_BYTECOUNT),
	.bInterval =		1 << LOG2_STATUS_INTERVAL_MSEC,
};
#endif

#define MULTIPLE_SETTINGS_SUPPORTED 1
static const struct usb_descriptor_header *fs_audio_function [] = {
	(struct usb_descriptor_header *) &otg_descriptor,
    (struct usb_descriptor_header *)&audio_ac_intf,
    (struct usb_descriptor_header *)&ac_interface_desc,

    (struct usb_descriptor_header *)&input_terminal_desc,
    (struct usb_descriptor_header *)&output_terminal_desc,
#ifdef FEATURE_UNIT_SUPPORTED
    (struct usb_descriptor_header *)&feature_unit_desc,
#endif
	(struct usb_descriptor_header *) &audio_streaming_intf_alt_0_desc,
	(struct usb_descriptor_header *) &audio_streaming_intf_alt_1_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_1_desc,
	(struct usb_descriptor_header *) &fs_out_ep_alt_1_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,
#ifdef MULTIPLE_SETTINGS_SUPPORTED
	(struct usb_descriptor_header *) &audio_streaming_intf_alt_2_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_2_desc,
	(struct usb_descriptor_header *) &fs_out_ep_alt_2_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,

	(struct usb_descriptor_header *) &audio_streaming_intf_alt_3_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_3_desc,
	(struct usb_descriptor_header *) &fs_out_ep_alt_3_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,

	(struct usb_descriptor_header *) &audio_streaming_intf_alt_4_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_4_desc,
	(struct usb_descriptor_header *) &fs_out_ep_alt_4_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,
#endif
	NULL,
};

#ifdef	CONFIG_USB_GADGET_DUALSPEED

/*
 * usb 2.0 devices need to expose both high speed and full speed
 * descriptors, unless they only run at full speed.
 *
 * that means alternate endpoint descriptors (bigger packets)
 * and a "device qualifier" ... plus more construction options
 * for the config descriptor.
 */

static struct usb_endpoint_descriptor
hs_out_ep_alt_1_desc = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize =   __constant_cpu_to_le16((1*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};
static struct usb_endpoint_descriptor
hs_out_ep_alt_2_desc = {
	.bLength =			USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize =   __constant_cpu_to_le16((2*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};

static struct usb_endpoint_descriptor
hs_out_ep_alt_3_desc = {
	.bLength =			USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize =   __constant_cpu_to_le16((3*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};

static struct usb_endpoint_descriptor
hs_out_ep_alt_4_desc = {
	.bLength =			USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	3,
	.bmAttributes =		USB_ENDPOINT_SYNC_SYNCRONOUS | USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize =   __constant_cpu_to_le16((4*OUT_EP_MAX_PACKET_SIZE)),
	.bInterval = 		4,
	.bRefresh = 		0,
};


static struct usb_qualifier_descriptor
dev_qualifier = {
	.bLength =		sizeof dev_qualifier,
	.bDescriptorType =	USB_DT_DEVICE_QUALIFIER,

	.bcdUSB =		__constant_cpu_to_le16 (0x0200),
	.bDeviceClass =		USB_CLASS_AUDIO,

	.bNumConfigurations =	1,
};


static const struct usb_descriptor_header *hs_audio_function [] = {
	(struct usb_descriptor_header *) &otg_descriptor,
    (struct usb_descriptor_header *)&audio_ac_intf,
    (struct usb_descriptor_header *)&ac_interface_desc,

    (struct usb_descriptor_header *)&input_terminal_desc,
    (struct usb_descriptor_header *)&output_terminal_desc,
#ifdef FEATURE_UNIT_SUPPORTED
    (struct usb_descriptor_header *)&feature_unit_desc,
#endif
	(struct usb_descriptor_header *) &audio_streaming_intf_alt_0_desc,
	(struct usb_descriptor_header *) &audio_streaming_intf_alt_1_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_1_desc,
	(struct usb_descriptor_header *) &hs_out_ep_alt_1_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,
#ifdef MULTIPLE_SETTINGS_SUPPORTED
	(struct usb_descriptor_header *) &audio_streaming_intf_alt_2_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_2_desc,
	(struct usb_descriptor_header *) &hs_out_ep_alt_2_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,

	(struct usb_descriptor_header *) &audio_streaming_intf_alt_3_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_3_desc,
	(struct usb_descriptor_header *) &hs_out_ep_alt_3_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,

	(struct usb_descriptor_header *) &audio_streaming_intf_alt_4_desc,
	(struct usb_descriptor_header *) &as_interface_desc,
	(struct usb_descriptor_header *) &as_format_alt_4_desc,
	(struct usb_descriptor_header *) &hs_out_ep_alt_4_desc,
	(struct usb_descriptor_header *) &as_iso_out_desc,
#endif
	NULL,
};

/* maxpacket and other transfer characteristics vary by speed. */
#define ep_desc(g,hs,fs) (((g)->speed==USB_SPEED_HIGH)?(hs):(fs))

#else

/* if there's no high speed support, maxpacket doesn't change. */
#define ep_desc(g,hs,fs) fs

#endif	/* !CONFIG_USB_GADGET_DUALSPEED */

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

static char				manufacturer [50];
static char				serial [40];

/* static strings, in UTF-8 */
static struct usb_string		strings [] = {
	{ STRING_MANUFACTURER, manufacturer, },
	{ STRING_PRODUCT, longname, },
	{ STRING_SERIAL, serial, },
	{ STRING_AUDIO, audio_gadget, },
	{  }			/* end of list */
};

static struct usb_gadget_strings	stringtab = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings,
};


static void
audio_suspend (struct usb_gadget *gadget)
{
	struct audio_dev	*dev = get_gadget_data (gadget);

	DBG (dev, "suspend\n");
	dev->suspended = 1;
}

static void
audio_resume (struct usb_gadget *gadget)
{
	struct audio_dev	*dev = get_gadget_data (gadget);

	DBG (dev, "resume\n");
	dev->suspended = 0;
}



static void audio_setup_complete (struct usb_ep *ep, struct usb_request *req)
{
	struct audio_dev *dev = (struct audio_dev *) ep->driver_data;
	if (req->status || req->actual != req->length)
		ERROR (dev, "setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
	else
	audio_intfreq_complete_count++;
}

#ifdef AUDIO_INTERFACE_REQUESTS_SUPPORTED
static int audio_set_intf_req(struct audio_dev *audio,
                const struct usb_ctrlrequest *ctrl)
{
    struct usb_request      *req = audio->req;  	//Used for control transfer
    u8                      id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
    u16                     len = le16_to_cpu(ctrl->wLength);
    u16                     w_value = le16_to_cpu(ctrl->wValue);
    u8                      con_sel = (w_value >> 8) & 0xFF;
    u8                      cmd = (ctrl->bRequest & 0x0F);
    struct usb_audio_control_selector *cs;
    struct usb_audio_control *con;

    DBG(audio, "bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
                        ctrl->bRequest, w_value, len, id);
    list_for_each_entry(cs, &audio->cs, list) {
        if (cs->id == id) {
            list_for_each_entry(con, &cs->control, list) {
                if (con->type == con_sel) {
                    audio->set_con = con;
                    break;
                }
            }
            break;
        }
    }
    audio->interface = id;
    req->context = audio;
    req->complete = audio_complete;

    return len;
}


static int audio_get_intf_req(struct audio_dev *audio,
                const struct usb_ctrlrequest *ctrl)
{
    struct usb_request      *req = audio->req;
    int                     value = -EOPNOTSUPP;
    u8                      id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
    u16                     len = le16_to_cpu(ctrl->wLength);
    u16                     w_value = le16_to_cpu(ctrl->wValue);
    u8                      con_sel = (w_value >> 8) & 0xFF;
    u8                      cmd = (ctrl->bRequest & 0x0F);

    struct usb_audio_control_selector *cs;
    struct usb_audio_control *con;

    DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
                    ctrl->bRequest, w_value, len, id);

    list_for_each_entry(cs, &audio->cs, list) {
            if (cs->id == id) {
                    list_for_each_entry(con, &cs->control, list) {
                            if (con->type == con_sel && con->get) {
                                    value = con->get(con, cmd);
                                    break;
                            }
                    }
                    break;
            }
    }
    req->context = audio;
    req->complete = audio_complete;
    memcpy(req->buf, &value, len);

    return len;
}

#endif



#define MAX_CONFIG_INTERFACES 2

static int audio_set_alt(struct audio_dev *, unsigned , unsigned );


static int
set_audio_config (struct audio_dev *dev, gfp_t gfp_flags)
{
	int			result = 0;
	int 			tmp;

#ifdef STATUS_INTERRUPT_EP_ENABLED
	/* status endpoint used for RNDIS and (optionally) CDC */
	if (dev->status_ep) {
		dev->status = ep_desc (dev->gadget, &hs_status_desc,
						&fs_status_desc);
		dev->status_ep->driver_data = dev;

		result = usb_ep_enable (dev->status_ep, dev->status);
		if (result != 0) {
			DBG (dev, "enable %s --> %d\n", 
				dev->status_ep->name, result);
			goto done;
		}
	}
#endif

	dev->out = ep_desc (dev->gadget, &hs_out_ep_alt_1_desc, &fs_out_ep_alt_1_desc);
        /* Initialize all interfaces by setting them to altsetting zero. */

		spin_lock (&dev->lock);
        for (tmp = 0; tmp < MAX_CONFIG_INTERFACES; tmp++) {

                result = audio_set_alt(dev, (unsigned)tmp, (unsigned)0);
                if (result < 0) {
                        DBG(dev, "interface %d alt 0 --> %d\n",
                                        tmp, result);

                        audio_reset_config(dev);
		                spin_unlock (&dev->lock);
                        goto done;
                }
        }

		spin_unlock (&dev->lock);
done:

#ifdef STATUS_INTERRUPT_EP_ENABLED
	if (result == 0)
		usb_ep_disable (dev->status_ep);
#endif
	/* on error, disable any endpoints  */
	if (result < 0) {
		dev->status = NULL;
		dev->out = NULL;
	}

	return result;
}



/* change our operational config.  must agree with the code
 * that returns config descriptors, and altsetting code.
 */
static int
audio_set_config (struct audio_dev *dev, unsigned number, gfp_t gfp_flags)
{
	int			result = 0;
	audio_reset_config (dev);
	
	switch (number) {
	case DEV_CONFIG_VALUE:
		result = set_audio_config (dev, gfp_flags);
		config_num = number;
		break;
	default:
		result = -EINVAL;
		/* FALL THROUGH */
	case 0:
		break;
	}
	if (result) {
		if (number)
			audio_reset_config (dev);
			usb_gadget_vbus_draw(dev->gadget,
				dev->gadget->is_otg ? 8 : 100);
	} else {
		unsigned power;

		power = 2 * audio_config.bMaxPower;
		usb_gadget_vbus_draw(dev->gadget, power);
		dev->config = number;
	}
	return result;
}



static int
config_buf (enum usb_device_speed speed,
	u8 *buf, u8 type,
	unsigned index, int is_otg)
{
	int					len;
	const struct usb_config_descriptor	*config;
	const struct usb_descriptor_header	**function;
#ifdef CONFIG_USB_GADGET_DUALSPEED
	int				hs = (speed == USB_SPEED_HIGH);

	if (type == USB_DT_OTHER_SPEED_CONFIG)
		hs = !hs;
#define which_fn(t)	(hs ? hs_ ## t ## _function : fs_ ## t ## _function)
#else
#define	which_fn(t)	(fs_ ## t ## _function)
#endif

	if (index >= device_desc.bNumConfigurations)
		return -EINVAL;

	config = &audio_config;
	function = which_fn (audio);

	/* for now, don't advertise srp-only devices */
	if (!is_otg)
		function++;

	len = usb_gadget_config_buf (config, buf, USB_BUFSIZ, function);
	if (len < 0)
		return len;
	((struct usb_config_descriptor *) buf)->bDescriptorType = type;
	return len;
}

static struct audio_buf *audio_buffer_alloc(int buf_size)
{
    struct audio_buf *copy_buf;

    copy_buf = kzalloc(sizeof *copy_buf, GFP_ATOMIC);
    if (!copy_buf)
            return (struct audio_buf *)-ENOMEM;

    copy_buf->buf = kzalloc(buf_size, GFP_ATOMIC);
    if (!copy_buf->buf) {
            kfree(copy_buf);
            return (struct audio_buf *)-ENOMEM;
    }

    return copy_buf;
}
	
//#define BUFDUMP_ENABLED_IN_CB 1
__attribute_used__ noinline static int audio_out_ep_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct audio_dev *audio = req->context;
	struct audio_buf *copy_buf = audio->copy_buf;
	int err;
	totalDatCnt +=req->actual;

    #ifdef BUFDUMP_ENABLED_IN_CB
    char *dest;
    char *src = (char *)req->buf;
    INFO (audio, "Audio ptr : %x Data count= %d fir byte = %x %x %x %x %x\n",audio, \
			(int)req->actual,src[0], src[1], src[2], src[3], src[20]);
    if(bufOff >= (1*1024*1024))
	return 0;
    dest = (audio_dump+bufOff);
    bufOff+=req->actual;
    memcpy(dest, src, req->actual);
    #else

	if (!copy_buf)
		return -EINVAL;
	/* Copy buffer is full, add it to the play_queue */
	if ((audio_buf_size - copy_buf->actual) < req->actual) {
		list_element_no++;
		memcpy(copy_buf->buf + copy_buf->actual, req->buf, (audio_buf_size - copy_buf->actual));
		copy_buf->actual += (audio_buf_size - copy_buf->actual);
		list_add_tail(&copy_buf->list, &audio->play_queue);
		schedule_work(&audio->playback_work);
		req->actual -= (audio_buf_size - copy_buf->actual);
		req->buf += (audio_buf_size - copy_buf->actual);

	#ifdef PLAYBACK_QUEUE_STATIC
		copy_buf = list_first_entry(&(audio->free_queue), struct audio_buf, list);
		list_del(&copy_buf->list);
	#else
		copy_buf = audio_buffer_alloc(audio_buf_size);
	#endif
		if (copy_buf < 0) {
            audio->copy_buf = NULL;
			return -ENOMEM;
        }
	}

	memcpy(copy_buf->buf + copy_buf->actual, req->buf, req->actual);
	copy_buf->actual += req->actual;
	audio->copy_buf = copy_buf;

    #endif	
	err = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (err)
		ERROR(audio, "%s queue req: %d\n", ep->name, err);

	return 0;

}

static void audio_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct audio_dev *audio = req->context;
    int status = req->status;
    struct usb_ep *out_ep = audio->out_ep;

	out_ep_complete_count++;
    switch (status) {

        case 0:                         /* normal completion? */
            if (ep == out_ep) {
	            spin_lock (&audio->lock);
                audio_out_ep_complete(ep, req);
	            spin_unlock (&audio->lock);
            }
            break;
        default:
            break;
    }
}

static void free_out_ep_reqs(struct audio_dev *audio)
{
    struct usb_request  *req;

	while (!list_empty(&audio->req_queue)) {
		req = list_first_entry(&(audio->req_queue), struct usb_request, list);
		list_del (&req->list);
		usb_ep_free_buffer (audio->out_ep, req->buf, req->dma, req->length);
		usb_ep_free_request (audio->out_ep, req);
	}
}
static int audio_set_alt(struct audio_dev *audio, unsigned intf, unsigned alt)
{
    struct usb_ep *out_ep = audio->out_ep;
    struct usb_request *req;
    int i = 0, err = 0;
    struct audio_buf *play_buf;

    if (intf == 1) {
        if (alt != 0) {
		    if(!(audio->copy_buf)) {
		    #ifdef PLAYBACK_QUEUE_STATIC
		    	play_buf = list_first_entry(&(audio->free_queue), struct audio_buf, list);
		    	list_del(&play_buf->list);
                audio->copy_buf = play_buf;
		    #else
                play_buf = audio_buffer_alloc(audio_buf_size);
	            if (play_buf < 0) {
                    audio->copy_buf = NULL;
		            return -ENOMEM;
                }
                audio->copy_buf = play_buf;
		    #endif
		    }
	    	if((1 != audio->urb_created) || (audio->altSetting != alt)) {
			    free_out_ep_reqs(audio);
			    audio->altSetting = (u8)alt;
			    audio->curAltSetting = (u8)alt;
			    out_ep_created++;
                switch(alt) {
                case 1:
                    audio->out = ep_desc (audio->gadget, &hs_out_ep_alt_1_desc, 
                                                 &fs_out_ep_alt_1_desc);
                    break;
                case 2:
                    audio->out = ep_desc (audio->gadget, &hs_out_ep_alt_2_desc, 
                                                 &fs_out_ep_alt_2_desc);
                    break;
                case 3:
                    audio->out = ep_desc (audio->gadget, &hs_out_ep_alt_3_desc, 
                                                 &fs_out_ep_alt_3_desc);
                    break;
                case 4:
                    audio->out = ep_desc (audio->gadget, &hs_out_ep_alt_4_desc, 
                                                 &fs_out_ep_alt_4_desc);
                    break;
                default:
                    ERROR (audio, "Invalid Alternate Setting: intf %d, alt %d\n", intf, alt);
                }

			    usb_ep_enable(out_ep, audio->out);
         	    out_ep->driver_data = audio;
         	    /*
          	    * allocate a bunch of read buffers
          	    * and queue them all at once.
          	    */
             	for (i = 0; i < req_count && err == 0; i++) {
                    req = usb_ep_alloc_request(out_ep, GFP_ATOMIC);
                    if (req) {

						req->buf = usb_ep_alloc_buffer (out_ep, (req_buf_size*alt),
							&req->dma, GFP_ATOMIC);
                        if (req->buf) {
                   	        req->length = req_buf_size*alt;
                            req->context = audio;
                            req->complete = audio_complete;
						    audio_set_alt_count++;
                            err = usb_ep_queue(out_ep,
                                       req, GFP_ATOMIC);
                            if (err)
                                ERROR(audio,
                                    "%s queue req: %d\n",
                                    out_ep->name, err);
                        } else
                            err = -ENOMEM;
                    } else
                         err = -ENOMEM;
				         list_add_tail(&req->list, &audio->req_queue);
                }
			    audio->urb_created = 1;
		    }
        } else {

           	struct audio_buf *copy_buf = audio->copy_buf;
		    audio->curAltSetting = alt;
            if (copy_buf) {
                if (copy_buf->actual != 0) {
			        list_element_no++;
                    #ifndef BUFDUMP_ENABLED_WQ
		    audio->copy_buf = NULL;
                    list_add_tail(&copy_buf->list, &audio->play_queue);
                    schedule_work(&audio->playback_work);
                    #endif
		        }
            }
   	    }
    }
    return err;
}


/*-------------------------------------------------------------------------*/

/**
 * Handle USB audio endpoint set/get command in setup class request
 */
#ifdef AUDIO_ENDPOINT_REQUESTS_SUPPORTED
static int audio_set_endpoint_req(const struct usb_ctrlrequest *ctrl)
{
    int                     value = -EOPNOTSUPP;
    u16                     ep = le16_to_cpu(ctrl->wIndex);
    u16                     len = le16_to_cpu(ctrl->wLength);
    u16                     w_value = le16_to_cpu(ctrl->wValue);


    switch (ctrl->bRequest) {
    case UAC_SET_CUR:
            value = 0;
            break;

    case UAC_SET_MIN:
            break;

    case UAC_SET_MAX:
            break;

    case UAC_SET_RES:
            break;

    case UAC_SET_MEM:
            break;

    default:
            break;
    }

    return value;
}

static int audio_get_endpoint_req(const struct usb_ctrlrequest *ctrl)
{
    int value = -EOPNOTSUPP;
    u8 ep = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
    u16 len = le16_to_cpu(ctrl->wLength);
    u16 w_value = le16_to_cpu(ctrl->wValue);

    switch (ctrl->bRequest) {
    case UAC_GET_CUR:
    case UAC_GET_MIN:
    case UAC_GET_MAX:
    case UAC_GET_RES:
            value = 3;
            break;
    case UAC_GET_MEM:
            break;
    default:
            break;
    }

    return value;
}
#endif

static int
audio_setup (struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct audio_dev	*dev = get_gadget_data (gadget);
	struct usb_request	*req = dev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* usually this stores reply data in the pre-allocated ep0 buffer,
	 * but config change events will reconfigure hardware.
	 */
	totalSetups++;
	req->zero = 0;
	g_bRequest = ctrl->bRequest;

	g_value = w_value;
	g_length = w_length;
	g_index = w_index;
	g_RequestType = ctrl->bRequestType;

	switch (ctrl->bRequest) {

	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != USB_DIR_IN)
			goto unknown;
		switch (w_value >> 8) {

		case USB_DT_DEVICE:
			value = min (w_length, (u16) sizeof device_desc);
			memcpy (req->buf, &device_desc, value);
                        count_get_dev_desc++;

			break;
#ifdef CONFIG_USB_GADGET_DUALSPEED
		case USB_DT_DEVICE_QUALIFIER:
			if (!gadget->is_dualspeed)
				break;
			value = min (w_length, (u16) sizeof dev_qualifier);
			memcpy (req->buf, &dev_qualifier, value);
			break;

		case USB_DT_OTHER_SPEED_CONFIG:
			if (!gadget->is_dualspeed)
				break;
			// FALLTHROUGH
#endif /* CONFIG_USB_GADGET_DUALSPEED */
		case USB_DT_CONFIG:
			req = dev->confreq; 
			value = config_buf (gadget->speed, req->buf,
					w_value >> 8,
					w_value & 0xff, 0);
			if (value >= 0)
				value = min (w_length, (u16) value);

                        count_get_config_desc++;

			break;

		case USB_DT_STRING:
			/* wIndex == language code.
			 * this driver only handles one language, you can
			 * add string tables for other languages, using
			 * any UTF-8 characters
			 */
			value = usb_gadget_get_string (&stringtab,
					w_value & 0xff, req->buf);
			if (value >= 0)
				value = min (w_length, (u16) value);
			break;
		}
		break;

	/* currently two configs, two speeds */
	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != 0)
			goto unknown;
		if (gadget->a_hnp_support)
			DBG (dev, "HNP available\n");
		else if (gadget->a_alt_hnp_support)
			DBG (dev, "HNP needs a different root port\n");
		else
			VDBG (dev, "HNP inactive\n");

                count_set_config++;
		spin_lock (&dev->lock);
		value = audio_set_config (dev, w_value, GFP_ATOMIC);
		spin_unlock (&dev->lock);
		break;
	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != USB_DIR_IN)
			goto unknown;
		*(u8 *)req->buf = dev->config;
		value = min (w_length, (u16) 1);
		break;

	/* until we add altsetting support, or other interfaces,
	 * only 0/0 are possible. 
	 */
	case USB_REQ_SET_INTERFACE:
		req_set_interface++;
		if (ctrl->bRequestType != USB_RECIP_INTERFACE)
			goto unknown;
		if (w_index >= MAX_CONFIG_INTERFACES)
			break;

		spin_lock (&dev->lock);
		value = audio_set_alt(dev, (unsigned)w_index, (unsigned)w_value);
		spin_unlock (&dev->lock);
		atomic_inc(&dev->contreq_status);
		break;
	case USB_REQ_GET_INTERFACE:

		req_get_interface++;
		if (ctrl->bRequestType != (USB_DIR_IN|USB_RECIP_INTERFACE))
			goto unknown;
		if (w_index >= MAX_CONFIG_INTERFACES)
			break;
		/* lots of interfaces only need altsetting zero... */
		spin_lock (&dev->lock);
		value = dev->curAltSetting;
		spin_unlock (&dev->lock);
		if (value < 0)
			break;
		*((u8 *)req->buf) = value;
		value = min(w_length, (u16) 1);
		break;

        case UAC_SET_CUR:
		if (ctrl->bRequestType != (USB_DIR_OUT|USB_TYPE_CLASS|USB_RECIP_INTERFACE))
			goto unknown;
                value = 0;
                break;


        case UAC_GET_CUR:
        case UAC_GET_MIN:
        case UAC_GET_MAX:
        case UAC_GET_RES:
		if (ctrl->bRequestType != (USB_DIR_IN|USB_TYPE_CLASS|USB_RECIP_INTERFACE))
			goto unknown;
                value = (int)w_length;
                break;

	default:
unknown:
		ERROR (dev,
			"unknown control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer before status phase? */
	if (value >= 0) {
		req->length = value;
		req->zero = value < w_length;
		req->status = 0;
		value = usb_ep_queue (gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			DBG (dev, "ep_queue --> %d\n", value);
			req->status = 0;
			audio_setup_complete (gadget->ep0, req);
		}
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}


static void free_ep_req (struct usb_ep *ep, struct usb_request *req)
{
	if (req->buf)
		usb_ep_free_buffer (ep, req->buf, req->dma, USB_BUFSIZ);
	usb_ep_free_request (ep, req);
}

u32 dst[MAX_AUDIO_CHAN][AUDIO_FRAME_SIZE/sizeof(u32)];
//#define BUFDUMP_ENABLED_WQ 1
__attribute_used__ noinline static int audio_playback(struct audio_dev *audio, void *buf, int count)
{
    int i, offset;
    int altSetting;
#ifdef WLAN_AOW_ENEBLED
    off_t tsf;
#endif


#ifdef BUFDUMP_ENABLED_WQ
    char *src = buf;
    char *dest;

    offset = (audio_buf_size-1);

    INFO (audio, "1, Data count= %d fir byte = %x %x %x %x %x\n",\
			count, src[0], src[100], src[2000], src[30000], src[offset]);

    if(bufOff >= (1*1024*1024))
	    return 0;

    dest = (audio_dump+bufOff);
    bufOff+=count;
    memcpy(dest, src, count);

    return 0;
#else
    u32 *src;
    int cnt, loop_count, extra;

    altSetting = (int)(audio->altSetting);
    count_audio_playback++;
    src = buf;

    if((altSetting >4) || (altSetting < 1)) {
        INFO (audio, "Error:: Alternate setting not correct\n\n");
        return 0;
    }

    if(count < audio_buf_size) {
	    cnt = count/(AUDIO_FRAME_SIZE*altSetting);
    	extra = count%(AUDIO_FRAME_SIZE);

    }
    else {
    	cnt = BUF_SIZE_FACTOR/altSetting;
	    extra = 0;
    }

    while (cnt--) {
  	    for (offset = 0; offset < (AUDIO_FRAME_SIZE/4); offset++) {
            for (i = 0; i < altSetting; i++)
                dst[i][offset] = *src++;
        }
#ifdef WLAN_AOW_ENEBLED
       	wlan_get_tsf(&tsf);
        for (i = 0; i < altSetting; i++) {
           wlan_aow_tx((char *)&(dst[i][0]), AUDIO_FRAME_SIZE, i, tsf);
        } 
#endif
    }

    if(extra != 0) {
	    cnt = extra/altSetting;
    	loop_count = cnt/4;

  	    for (offset = 0; offset < loop_count; offset++) {
            for (i = 0; i < altSetting; i++) {
                dst[i][offset] = *src++;
            }
        }
	
#ifdef WLAN_AOW_ENEBLED
      	for (i = 0; i < altSetting; i++) {
    		wlan_aow_tx((char *)&(dst[i][0]), cnt, i, tsf);
    	}
#endif
    }
#endif
    return 0;
}



static void audio_buffer_free(struct audio_buf *audio_buf)
{
	kfree(audio_buf->buf);
	kfree(audio_buf);
}



static void audio_playback_work(struct work_struct *data)
{
	struct audio_dev *audio = container_of(data, struct audio_dev,
					playback_work);
	struct audio_buf *play_buf;

	spin_lock_irq(&audio->lock);
	if (list_empty(&audio->play_queue)) {
		spin_unlock_irq(&audio->lock);
		return;
	}
	play_buf = list_first_entry(&(audio->play_queue), struct audio_buf, list);
	list_element_no--;
	list_del(&play_buf->list);
	spin_unlock_irq(&audio->lock);

	audio_playback(audio, play_buf->buf, play_buf->actual);
#ifdef PLAYBACK_QUEUE_STATIC
	list_add_tail(&play_buf->list, &audio->free_queue);
#else
	audio_buffer_free(play_buf);
#endif
	return;
}


static void
audio_unbind (struct usb_gadget *gadget)
{
	struct audio_dev	*dev = get_gadget_data (gadget);

	DBG (dev, "unbind\n");

	/* we've already been disconnected ... no i/o is active */
   	free_out_ep_reqs(dev);
	if (dev->req)
		free_ep_req (gadget->ep0, dev->req);
    dev->req = NULL;
    cancel_delayed_work(&dev->playback_work);
	kfree (dev);
	set_gadget_data (gadget, NULL);
}



static int
audio_bind (struct usb_gadget *gadget)
{
	struct audio_dev	*dev;
	struct usb_ep		*ep;
    #ifdef PLAYBACK_QUEUE_STATIC
	struct audio_buf 	*copy_buf;
    int                 i;
    #endif
	int			gcnum;

	usb_ep_autoconfig_reset (gadget);
	ep = usb_ep_autoconfig (gadget, ep_desc (gadget, &hs_out_ep_alt_1_desc, &fs_out_ep_alt_1_desc));
	if (!ep) {
		printk (KERN_ERR "%s: can't autoconfigure on %s\n",
			shortname, gadget->name);
		return -ENODEV;
	}
	EP_IN_NAME = ep->name;
	ep->driver_data = ep;	/* claim */


	gcnum = usb_gadget_controller_number (gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16 (0x0200 + gcnum);
	else {
		/* gadget zero is so simple (for now, no altsettings) that
		 * it SHOULD NOT have problems with bulk-capable hardware.
		 * so warn about unrcognized controllers, don't panic.
		 *
		 * things like configuration and altsetting numbering
		 * can need hardware-specific attention though.
		 */
		printk (KERN_WARNING "%s: controller '%s' not recognized\n",
			shortname, gadget->name);
		device_desc.bcdDevice = __constant_cpu_to_le16 (0x9999);
	}

	/* ok, we made sense of the hardware ... */
	dev = kmalloc (sizeof *dev, SLAB_KERNEL);
	if (!dev)
		return -ENOMEM;
	memset (dev, 0, sizeof *dev);
	spin_lock_init (&dev->lock);
	dev->gadget = gadget;
	dev->altSetting = (u8)1;
	set_gadget_data (gadget, dev);
	dev->out_ep = ep;
	INIT_LIST_HEAD(&dev->play_queue);
	INIT_LIST_HEAD(&dev->req_queue);
	atomic_set(&dev->contreq_status, 0);
#ifdef PLAYBACK_QUEUE_STATIC
	INIT_LIST_HEAD(&dev->free_queue);
	for(i = 0; i < BUFFER_SIZE; i++)
	{
		copy_buf = audio_buffer_alloc(audio_buf_size);
		if (!copy_buf)
			return -ENOMEM;
		list_add_tail(&copy_buf->list, &dev->free_queue);
		
	}
#endif
	/* preallocate control response and buffer */
	dev->req = usb_ep_alloc_request (gadget->ep0, GFP_KERNEL);
	if (!dev->req)
		goto enomem;
	dev->req->buf = usb_ep_alloc_buffer (gadget->ep0, USB_BUFSIZ,
				&dev->req->dma, GFP_KERNEL);
	if (!dev->req->buf)
		goto enomem;

	dev->req->complete = audio_setup_complete;

	dev->confreq= usb_ep_alloc_request (gadget->ep0, GFP_KERNEL);
	if (!dev->confreq)
		goto enomem;
	dev->confreq->buf = usb_ep_alloc_buffer (gadget->ep0, USB_BUFSIZ,
				&dev->confreq->dma, GFP_KERNEL);
	if (!dev->confreq->buf)
		goto enomem;

	dev->confreq->complete = audio_setup_complete;

	device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;

#ifdef CONFIG_USB_GADGET_DUALSPEED
	/* assume ep0 uses the same value for both speeds ... */
	dev_qualifier.bMaxPacketSize0 = device_desc.bMaxPacketSize0;

	/* and that all endpoints are dual-speed */
	hs_out_ep_alt_1_desc.bEndpointAddress = fs_out_ep_alt_1_desc.bEndpointAddress;
#endif

	if (gadget->is_otg) {
		otg_descriptor.bmAttributes |= USB_OTG_HNP,
		audio_config.bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	if (gadget->is_otg) {
		otg_descriptor.bmAttributes |= USB_OTG_HNP,
		audio_config.bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	usb_gadget_set_selfpowered (gadget);

	INIT_WORK(&dev->playback_work, audio_playback_work, &dev->playback_work);

	gadget->ep0->driver_data = dev;

	INFO (dev, "%s, version: " DRIVER_VERSION "\n", longname);
	INFO (dev, "using %s, OUT %s IN %s\n", gadget->name,
		EP_OUT_NAME, EP_IN_NAME);

	snprintf (manufacturer, sizeof manufacturer, "%s %s with %s",
		system_utsname.sysname, system_utsname.release,
		gadget->name);

	return 0;

enomem:
	audio_unbind (gadget);
	return -ENOMEM;
}

static void audio_reset_config (struct audio_dev *dev)
{
	if (dev->config == 0)
		return;

	DBG (dev, "reset config\n");

	/* just disable endpoints, forcing completion of pending i/o.
	 * all our completion handlers free their requests in this case.
	 */
	if (dev->out_ep) {
		usb_ep_disable (dev->out_ep);
//		dev->out_ep = NULL;
	}
	dev->config = 0;
}



static void
audio_disconnect (struct usb_gadget *gadget)
{
	struct audio_dev		*dev = get_gadget_data (gadget);
	unsigned long		flags;

	spin_lock_irqsave (&dev->lock, flags);
	audio_reset_config (dev);
	dev->urb_created = 0;

	/* a more significant application might have some non-usb
	 * activities to quiesce here, saving resources like power
	 * or pushing the notification up a network stack.
	 */
	spin_unlock_irqrestore (&dev->lock, flags);

	/* next we may get setup() calls to enumerate new connections;
	 * or an unbind() during shutdown (including removing module).
	 */
}

static unsigned long readOff = 0;
static int gaudio_readdata_procmem(char *buf, char **start, off_t offset,
                                int count, int *eof, void *data)
{
    int i, len = 0;
    char *src;
    if(count <= 10)
    {
	bufOff = 0;
	readOff = 0;
	return 0;
    }
    #ifdef BUFDUMP_ENABLED_WQ
    src = (audio_dump+readOff);
    if((readOff >= (1*1024*1024)) || (readOff >= bufOff))
	return 0;
    if((readOff+count) > bufOff)
	count = bufOff-readOff;
    for(i=0; len < count; i++) {
    	len += sprintf(buf+len,"%x", src[i]);
    }
    readOff+=count;
    #else

  	for (i = 0; i < 4; i++) {
    for (len = 0; len < OUT_EP_MAX_PACKET_SIZE; len++)
    	count += sprintf(buf+len,"%x", dst[i][len]);
    	count += sprintf(buf+len,"\nDumping alt set = %d\n", i);
    }
    #endif
    return count;
}

static int gaudio_read_procmem(char *buf, char **start, off_t offset,
                                int count, int *eof, void *data)
{

    int len = 0;
    len += sprintf(buf+len,"\nNumber of set_config= %li\n", count_set_config);
    len += sprintf(buf+len,"\nNumber of get_config_desc= %li\n", count_get_config_desc);
    len += sprintf(buf+len,"\nNumber of get_device_desc= %li\n", count_get_dev_desc);
    len += sprintf(buf+len,"\nNumber of config number = %u\n", config_num);
    len += sprintf(buf+len,"\nRequest Type = %x, Request number = %x, Value = %x, Index = %u, \
			Length = %u\n", g_RequestType, g_bRequest, g_value, g_index, g_length);
    len += sprintf(buf+len,"\nRequest set interface = %li\n", req_set_interface);
    len += sprintf(buf+len,"\nRequest set interface(audio_set_alt) = %li\n", audio_set_alt_count);
    len += sprintf(buf+len,"\nRequest get interface = %li\n", req_get_interface);
    len += sprintf(buf+len,"\nTotal setup requests = %li\n", totalSetups);
    len += sprintf(buf+len,"\nTotal data transfer complete= %li\n", out_ep_complete_count);
    len += sprintf(buf+len,"\nTotal control req transfer complete= %li\n", audio_intfreq_complete_count);
    len += sprintf(buf+len,"\nNodes in the list = %d\n", list_element_no);
    len += sprintf(buf+len,"\nAudio playback count = %li\n", count_audio_playback);
    len += sprintf(buf+len,"\nTotal data came = %li\n", totalDatCnt);
    len += sprintf(buf+len,"\nOut EP created= %li\n", out_ep_created);
    len += sprintf(buf+len,"\nRead Offset= %li, Buffer offset = %li\n", readOff, bufOff);

    return len;
}

/*-------------------------------------------------------------------------*/

static struct usb_gadget_driver audio_driver = {
#ifdef CONFIG_USB_GADGET_DUALSPEED
	.speed		= USB_SPEED_HIGH,
#else
	.speed		= USB_SPEED_FULL,
#endif
	.function	= (char *) longname,
	.bind		= audio_bind,
	.unbind		= audio_unbind,

	.setup		= audio_setup,
	.disconnect	= audio_disconnect,

	.suspend	= audio_suspend,
	.resume		= audio_resume,

	.driver 	= {
		.name		= (char *) shortname,
		.owner		= THIS_MODULE,
		// .shutdown = ...
		// .suspend = ...
		// .resume = ...
	},
};

MODULE_AUTHOR ("Nitish@Atheros");
MODULE_LICENSE ("Dual BSD/GPL");


static int __init init (void)
{
	/* a real value would likely come through some id prom
	 * or module option.  this one takes at least two packets.
	 */
	strlcpy (serial, "0123456789.0123456789.0123456789", sizeof serial);
    create_proc_read_entry("gaudio", 0, NULL, gaudio_read_procmem, NULL);
    create_proc_read_entry("auddata", 0, NULL, gaudio_readdata_procmem, NULL);
    #ifdef BUFDUMP_ENABLED_WQ
	audio_dump = vmalloc(1*1024*1024);
    #endif

	return usb_gadget_register_driver (&audio_driver);
}
module_init (init);

static void __exit cleanup (void)
{
    #ifdef BUFDUMP_ENABLED_WQ
	vfree(audio_dump);
    #endif
	remove_proc_entry("gaudio", NULL);
	remove_proc_entry("auddata", NULL);
	usb_gadget_unregister_driver (&audio_driver);
}
module_exit (cleanup);

