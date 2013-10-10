#ifndef USB_13320A_DEFS_H
#define USB_13320A_DEFS_H

/*
 * These registers/address define are referred to
 * 1. CI13420_CI13520_CI13620_CI13720_IPCS_PM_HighSpeedControllerCore_20A.pdf
 * 2. Virian_Spec.pdf
 */

#define BASE_ADDRESS				0xA0000000
#define RESET_BASE_ADDRESS			(BASE_ADDRESS+0x18060000)
#define BOOT_TYPE_REGISTER			(0x00000008 + RESET_BASE_ADDRESS)
#define RESET_REGISTER				(0x0000001C + RESET_BASE_ADDRESS)

// WatchDog reset or COLD start indicator
#define BOOT_TYPE_REGISTER_WATCHDOG_RESET	(1<<31)

// Wasp RESET register bits defines
#define RESET_USB_PHY_ARESET		(1 << 11) // Phy Analog reset
#define RESET_USB_HOST_RESET		(1 << 5)
#define RESET_USB_PHY_RESET		(1 << 4)
#define RESET_SUSPEND_OVERRIDE		(1 << 3)

#define REG_WLAN_RTC                    0xb8107040
#define REG_WLAN_RTC_SYNC_STATUS        0xb8107044
#define USB_PHY_CTRL2                   0xb8116C88

#define RTC_SYNC_RESET_L                (1<<0)
#define RTC_SYNC_STATUS_ON_STATE        (1<<1)
#define USB_PHY_CTRL2_TX_MAN_CAL_SHIFT  (22)
#define USB_PHY_CTRL2_TX_MAN_CAL_MASK   0xFC3FFFFF  //clear bit [25:22]
#define USB_PHY_CTRL2_TX_CAL_SEL        (1<<21)
#define USB_PHY_CTRL2_TX_CAL_EN         (1<<20)

#define	ATH_USB_CONFIG			(BASE_ADDRESS + 0x18030004)
#define	ATH_HOST_OR_DEV			(1u << 4)
#define	ATH_HS_MODE_EN			(1u << 2)

// CI 13320A registers addr defines
#define CI13320A_USB_BASE		(BASE_ADDRESS+0x1b000000)
#define CI13320A_USB_USBCMD		(CI13320A_USB_BASE + 0x140)
#define CI13320A_USB_USBSTS		(CI13320A_USB_BASE + 0x144)
#define CI13320A_USB_USBINTR		(CI13320A_USB_BASE + 0x148)
#define CI13320A_USB_FRINDEX		(CI13320A_USB_BASE + 0x14C)
#define CI13320A_USB_PERIODICLISTBASE	(CI13320A_USB_BASE + 0x154)	//Host Mode
#define CI13320A_USB_DEVICEADDR		(CI13320A_USB_BASE + 0x154)	//Device Mode
#define CI13320A_USB_ASYNCLISTADDR	(CI13320A_USB_BASE + 0x158)	//Host Mode
#define CI13320A_USB_ENDPOINTLISTADDR	(CI13320A_USB_BASE + 0x158)	//Device Mode
#define CI13320A_USB_TTCTRL		(CI13320A_USB_BASE + 0x15C)
#define CI13320A_USB_BURSTSIZE		(CI13320A_USB_BASE + 0x160)
#define CI13320A_USB_TXFILLTUNING	(CI13320A_USB_BASE + 0x164)
#define CI13320A_USB_TXTTFILLTUNING	(CI13320A_USB_BASE + 0x168)
#define CI13320A_USB_IC_USB		(CI13320A_USB_BASE + 0x16C)
#define CI13320A_USB_ULPI_VIEWPORT	(CI13320A_USB_BASE + 0x170)
#define CI13320A_USB_ENDPTNAK		(CI13320A_USB_BASE + 0x178)
#define CI13320A_USB_ENDPTNAKEN		(CI13320A_USB_BASE + 0x17C)
#define CI13320A_USB_CONFIGFLAG		(CI13320A_USB_BASE + 0x180)
#define CI13320A_USB_PORTSC0		(CI13320A_USB_BASE + 0x184)
#define CI13320A_USB_OTGSC		(CI13320A_USB_BASE + 0x1A4)
#define CI13320A_USB_USBMODE		(CI13320A_USB_BASE + 0x1A8)
#define CI13320A_USB_ENDPTSETUPSTAT	(CI13320A_USB_BASE + 0x1AC)
#define CI13320A_USB_ENDPTPRIME		(CI13320A_USB_BASE + 0x1B0)
#define CI13320A_USB_ENDPTFLUSH		(CI13320A_USB_BASE + 0x1B4)
#define CI13320A_USB_ENDPTSTAT		(CI13320A_USB_BASE + 0x1B8)
#define CI13320A_USB_ENDPTCOMPLETE	(CI13320A_USB_BASE + 0x1BC)
#define CI13320A_USB_ENDPTCTRL0		(CI13320A_USB_BASE + 0x1C0)
#define CI13320A_USB_ENDPTCTRL1		(CI13320A_USB_BASE + 0x1C4)
#define CI13320A_USB_ENDPTCTRL2		(CI13320A_USB_BASE + 0x1C8)
#define CI13320A_USB_ENDPTCTRL3		(CI13320A_USB_BASE + 0x1CC)
#define CI13320A_USB_ENDPTCTRL4		(CI13320A_USB_BASE + 0x1D0)
#define CI13320A_USB_ENDPTCTRL5		(CI13320A_USB_BASE + 0x1D4)
#define CI13320A_USB_ENDPTCTRL6		(CI13320A_USB_BASE + 0x1D8)
#define CI13320A_USB_ENDPTCTRL7		(CI13320A_USB_BASE + 0x1DC)
#define CI13320A_USB_ENDPTCTRL8		(CI13320A_USB_BASE + 0x1E0)

// USBCMD bits define
#define CI13320A_USBCMD_SETUP_TRIPWIRE_SET	(0x00002000)
#define CI13320A_USBCMD_SETUP_TRIPWIRE_CLEAR	~(0x00002000)
#define CI13320A_USBCMD_ATDTW_TRIPWIRE_SET	(0x00004000)
#define CI13320A_USBCMD_ATDTW_TRIPWIRE_CLEAR	~(0x00004000)
#define CI13320A_USBCMD_RESET			(1<<1)
#define CI13320A_USBCMD_RUN			(1<<0)

// USBSTS bits define
#define CI13320A_USBSTS_GPTIMER_INTR1            (1<<25)
#define CI13320A_USBSTS_GPTIMER_INTR0            (1<<24)
#define CI13320A_USBSTS_HOST_PERIODIC_INTR       (1<<19)
#define CI13320A_USBSTS_HOST_ASYNC_INTR          (1<<18)
#define CI13320A_USBSTS_NAT_INTR                 (1<<16)
#define CI13320A_USBSTS_ASYNC_SCHEDULE_STAT      (1<<15)
#define CI13320A_USBSTS_PERIODIC_SCHEDULE_STAT   (1<<14)
#define CI13320A_USBSTS_RECLAMATION              (1<<13)
#define CI13320A_USBSTS_HCHALTED                 (1<<12)
#define CI13320A_USBSTS_ULPIINTR                 (1<<10)
#define CI13320A_USBSTS_DCSUSPEND                (1<<8)
#define CI13320A_USBSTS_SOFRECV                  (1<<7)
#define CI13320A_USBSTS_RESETRECV                (1<<6)
#define CI13320A_USBSTS_INTR_ASYNCADV            (1<<5)
#define CI13320A_USBSTS_SYS_ERR                  (1<<4)
#define CI13320A_USBSTS_FRAMELIST_ROLLOVER       (1<<3)
#define CI13320A_USBSTS_PORT_CHANGE              (1<<2)
#define CI13320A_USBSTS_ERROR_INTERRUPT          (1<<1)
#define CI13320A_USBSTS_INTR                     (1<<0)

// USBINTR bits define
#define CI13320A_USBINTR_GPTIMER_INTR1            (1<<25)
#define CI13320A_USBINTR_GPTIMER_INTR0            (1<<24)
#define CI13320A_USBINTR_HOST_PERIODIC_INTR       (1<<19)
#define CI13320A_USBINTR_HOST_ASYNC_INTR          (1<<18)
#define CI13320A_USBINTR_NAK_INTR                 (1<<16)
#define CI13320A_USBINTR_ULPIINTR                 (1<<10)
#define CI13320A_USBINTR_DCSUSPEND                (1<<8)
#define CI13320A_USBINTR_SOFRECV                  (1<<7)
#define CI13320A_USBINTR_RESETRECV                (1<<6)
#define CI13320A_USBINTR_INTR_ASYNCADV            (1<<5)
#define CI13320A_USBINTR_SYS_ERR                  (1<<4)
#define CI13320A_USBINTR_FRAMELIST_ROLLOVER       (1<<3)
#define CI13320A_USBINTR_PORT_CHANGE_INTR         (1<<2)
#define CI13320A_USBINTR_ERROR_INTR               (1<<1)
#define CI13320A_USBINTR_INTR                     (1<<0)

//USB DEVICE ADDRE
#define CI13320A_USBDEVICE_ADDR_SHIFT	     	  (25)
#define CI13320A_USBDEVICE_ADDR_ADVANCE	     	  (1<<24)

// USBMODE bits define
#define CI13320A_USBMODE_SRT			     	  (1<<15)
#define CI13320A_USBMODE_VBPS			     	  (1<<5)
#define CI13320A_USBMODE_SDIS			     	  (1<<4)
#define CI13320A_USBMODE_SLOM			     	  (1<<3)
#define CI13320A_USBMODE_ENDIAN					  (1<<2)
#define CI13320A_USBMODE_DEVICE					  (0x2)
#define CI13320A_USBMODE_HOST					  (0x3)

// USBSETUPSTAT bits define
#define CI13320A_USBSETUPSTAT_ENDPT5			  (1<<5)
#define CI13320A_USBSETUPSTAT_ENDPT4			  (1<<4)
#define CI13320A_USBSETUPSTAT_ENDPT3			  (1<<3)
#define CI13320A_USBSETUPSTAT_ENDPT2			  (1<<2)
#define CI13320A_USBSETUPSTAT_ENDPT1			  (1<<1)
#define CI13320A_USBSETUPSTAT_ENDPT0			  (1<<0)

// USBENDPTPRIME bits define
#define CI13320A_USBENDPTPRIME_ENDPT3_TX	      (1<<19)
#define CI13320A_USBENDPTPRIME_ENDPT2_TX	      (1<<18)
#define CI13320A_USBENDPTPRIME_ENDPT1_TX	      (1<<17)
#define CI13320A_USBENDPTPRIME_ENDPT0_TX	      (1<<16)
#define CI13320A_USBENDPTPRIME_ENDPT2_RX		  (1<<2)
#define CI13320A_USBENDPTPRIME_ENDPT1_RX		  (1<<1)
#define CI13320A_USBENDPTPRIME_ENDPT0_RX		  (1<<0)

// CI13320A_USB_PORTSC0 bits define
#define CI13320A_USB_PORTSC0_HSP	  			  (1<<9)
#define CI13320A_USB_PORTSC0_SUSP				  (1<<7)
#define TEST_MODE_J_STATE						  (1)
#define TEST_MODE_K_STATE						  (2)
#define TEST_MODE_SE0_NAK						  (3)
#define TEST_MODE_TEST_PACKET					  (4)
#define CI13320A_USB_PORTSC0_TEST_MODE			  (16)

// CI13320A_USB_ENDPTCTRL bits define
#define CI13320A_USB_ENDPTCTRL_TX_ENABLE               (1<<23)
#define CI13320A_USB_ENDPTCTRL_TX_DATA_TOGGLE_RESET    (1<<22)
#define CI13320A_USB_ENDPTCTRL_TX_TRANSFER_MODE_SHIFT   (18)
#define CI13320A_USB_ENDPTCTRL_RX_ENABLE                (1<<7)
#define CI13320A_USB_ENDPTCTRL_RX_DATA_TOGGLE_RESET     (1<<6)
#define CI13320A_USB_ENDPTCTRL_RX_TRANSFER_MODE_SHIFT   (2)


/*------------------------------
 *   PLL Registers Define
 ------------------------------*/
#define PLL_BASE_ADDRESS			     	(BASE_ADDRESS+0x18050000)
#define PLL_CONFIG_REG						(PLL_BASE_ADDRESS)
#define PLL_CLK_CTRL						(PLL_BASE_ADDRESS+0x8)
#define PLL_SUSPEND							(PLL_BASE_ADDRESS+0x40)

//PLL_CONFIG_REG
#define PLL_CONFIG_REG_PLLPWD					  (1<<30)
#define PLL_CONFIG_REG_UPDATING					  (1<<31)
//PLL_CLK_CTRL
#define PLL_CLK_CTRL_BYPASS					       (1<<2)
//PLL_SUSPEND
#define PLL_SUSPEND_ENABLE						   (1<<0)


// USB Generic Define
#define PAGE_4K					  				   (4096)
#define QH_SIZE							    		 (64)
#define DTD_SIZE  								     (64)  //Let it be 32 byte alignment
#define DTD_PER_ENDPT								(1)
#define BUF_PER_DTD                                 (1)
#define DTD_BUF_SIZE						  (BUF_PER_DTD*PAGE_4K)
#define USB_ENDPT_NUM                                5 //not include endpt 0

#define CI13320A_DTD_TERMINATE                      (1)
#define CI13320A_DTD_IOC                          (1<<15)
#define CI13320A_DTD_STATUS_ACTIVE                (1<<7)
#define CI13320A_DTD_STATUS_HALTED                (1<<6)
#define CI13320A_DTD_STATUS_DATA_ERR              (1<<5)
#define CI13320A_DTD_STATUS_TRAN_ERR              (1<<3)


#define CI13320A_QH_ZLENGTH      (1<<29)
#define CI13320A_QH_IOS          (1<<15)
#define CI13320A_QH_SIZESHIFT    (16)

#define CI13320A_DTD_TOTALBYTESSHFIT    (16)
#define CI13320A_DTD_ACTIVE              (1<<7)

// Vendor Commands for Firmware Download
#define SETUP_VENDOR_REQUEST_DL           0x40
#define SETUP_VENDOR_REQUEST_CMP          0xC0
#define FIRMWARE1_DOWNLOAD                0x30
#define FIRMWARE1_DOWNLOAD_COMP           0x31
#define FIRMWARE2_DOWNLOAD                0x32
#define FIRMWARE2_DOWNLOAD_COMP           0x33

// Size for SPEC

/*  Data Structures */
typedef struct _udc UDC;

/* End Point Queue Head chipIdea Sec-3.1, little endian */
typedef struct ep_qhead {
    __s32  maxPacketLen;
    __s32  curr_dtd;
    __s32  next_dtd;
    __s32  size_ioc_status;
    __s32  buff[5];
    __s32  resv;
    __u8   setup_buff[8];
    __u32  resv1[4];
} EP_QH;

/* End Point Transfer Desc chipIdea Sec-3.2, little endian */
typedef struct ep_dtd {
    /* Hardware access fields */
    __s32  next_dtd;
    __s32  size_ioc_status;
    __s32  buff[5];
    /* SW field */
    struct ep_dtd  *next;
    __u32   count;
} EP_DTD;

/* End Point Data Structure */
typedef struct ep {
    EP_QH *ep_qh;
    __u32 halt;
    UDC *udc;
}EP;

/* USB Device Controller Data Structure */
struct _udc{
    __u32               devAddr;
    __u32               total_dtd_number;
    __u32               free_dtd_number;
    __u32               suspend_stat;
    __u32               start_type;
    __u32               configuration;
    __u32               dev_status;
    __u8                read_from_otp;
    __u8                watchdog_notify;
    struct ep           ep[USB_ENDPT_NUM+1];   //including endpt0
};

typedef struct _transfer_unit {
    EP_QH *qh;
    EP_DTD *dtd;
}TRANSFER_UNIT;

typedef struct _endpt0
{
    TRANSFER_UNIT rx;
    TRANSFER_UNIT tx;
}ENDPT0;
#endif

