/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _HIF_USB_H_
#define _HIF_USB_H_

/**
 * @file hif.h
 * HostInterFace layer.
 * Abstracts interface-dependent transmit/receive functionality.
 */ 

#define urb_t                       struct urb

#ifndef __ahdecl
#ifdef __i386__
#define __ahdecl    __attribute__((regparm(0)))
#else
#define __ahdecl
#endif
#endif

#define HIF_USB_PIPE_COMMAND            4 /* 0 */
#define HIF_USB_PIPE_INTERRUPT          3 /* 1 */
#define HIF_USB_PIPE_TX                         1 /* 2 */
#define HIF_USB_PIPE_RX                         2 /* 3 */
#define HIF_USB_PIPE_HP_TX                      5
#define HIF_USB_PIPE_MP_TX                      6


#define HIF_USB_MAX_RXPIPES     2
#define HIF_USB_MAX_TXPIPES     4



/* USB Endpoint definition */
#define USB_WLAN_TX_PIPE                    1
#define USB_WLAN_RX_PIPE                    2
#define USB_REG_IN_PIPE                     3
#define USB_REG_OUT_PIPE                    4
#define USB_WLAN_HP_TX_PIPE                 5
#define USB_WLAN_MP_TX_PIPE                 6

#define FIRMWARE_DOWNLOAD       0x30
#define FIRMWARE_DOWNLOAD_COMP  0x31
#define FIRMWARE_CONFIRM        0x32
#define ZM_FIRMWARE_WLAN_ADDR               0x501000
#define ZM_FIRMWARE_TEXT_ADDR		        0x903000
#define ZM_FIRMWARE_MAGPIE_TEXT_ADDR		0x906000 

#define ZM_MAX_RX_BUFFER_SIZE               16384
//#define ZM_DONT_COPY_RX_BUFFER              1

#define ZM_USB_STREAM_MODE      1
#define ZM_USB_TX_STREAM_MODE 1
#if ZM_USB_TX_STREAM_MODE == 1
#define ZM_MAX_TX_AGGREGATE_NUM             20
#define ZM_USB_TX_BUF_SIZE                  32768
#ifdef BUILD_X86
#define ZM_MAX_TX_URB_NUM                   4             /* Offload the HIF queue effort to HCD. Compare to PB44 solution, URB recycle timing is more slower 
                                                             in X86 solution and this make HIF queue is full frequently then start to drop packet. */
#else
#define ZM_MAX_TX_URB_NUM                   2
#endif
#define ZM_L1_TX_AGGREGATE_NUM              10
#define ZM_L1_TX_PACKET_THR                 5000
#define ZM_1MS_SHIFT_BITS                   18
#else
#define ZM_USB_TX_BUF_SIZE                  2048
#define ZM_MAX_TX_URB_NUM                   8
#endif

#define ZM_USB_REG_MAX_BUF_SIZE             64  // = MAX_MSGIN_BUF_SIZE
#define ZM_MAX_RX_URB_NUM                   2
#define ZM_MAX_TX_BUF_NUM                   256 //1024
#define ZM_MAX_TX_BUF_NUM_K2                128

#define ZM_MAX_USB_IN_NUM                   16

#define MAX_CMD_URB                         12   //4

#define ZM_RX_URB_BUF_ALLOC_TIMER           100

//#define ATH_USB_STREAM_MODE_TAG_LEN         4
#define ATH_USB_STREAM_MODE_TAG             0x4e00
//#define ATH_MAX_USB_IN_TRANSFER_SIZE        8192
//#define AHT_MAX_PKT_NUM_IN_TRANSFER         8

//#define ATH_USB_MAX_BUF_SIZE        8192
//#define ATH_MAX_TX_AGGREGATE_NUM    3
#define ZM_MAX_USB_URB_FAIL_COUNT             1

/* define EVENT ID for Worker Queue */
#define ATH_RECOVER_EVENT                     1

typedef enum hif_status {
    HIF_OK = 0,
    HIF_ERROR = -1
}hif_status_t;


typedef enum {
    A_ERROR = -1,               /* Generic error return */
    A_OK = 0,                   /* success */
                                /* Following values start at 1 */
    A_DEVICE_NOT_FOUND,         /* not able to find PCI device */
    A_NO_MEMORY,                /* not able to allocate memory, not available */
    A_MEMORY_NOT_AVAIL,         /* memory region is not free for mapping */
    A_NO_FREE_DESC,             /* no free descriptors available */
    A_BAD_ADDRESS,              /* address does not match descriptor */
    A_WIN_DRIVER_ERROR,         /* used in NT_HW version, if problem at init */
    A_REGS_NOT_MAPPED,          /* registers not correctly mapped */
    A_EPERM,                    /* Not superuser */
    A_EACCES,                   /* Access denied */
    A_ENOENT,                   /* No such entry, search failed, etc. */
    A_EEXIST,                   /* The object already exists (can't create) */
    A_EFAULT,                   /* Bad address fault */
    A_EBUSY,                    /* Object is busy */
    A_EINVAL,                   /* Invalid parameter */
    A_EMSGSIZE,                 /* Inappropriate message buffer length */
    A_ECANCELED,                /* Operation canceled */
    A_ENOTSUP,                  /* Operation not supported */
    A_ECOMM,                    /* Communication error on send */
    A_EPROTO,                   /* Protocol error */
    A_ENODEV,                   /* No such device */
    A_EDEVNOTUP,                /* device is not UP */
    A_NO_RESOURCE,              /* No resources for requested operation */
    A_HARDWARE,                 /* Hardware failure */
    A_PENDING,                  /* Asynchronous routine; will send up results la
ter (typically in callback) */
    A_EBADCHANNEL,              /* The channel cannot be used */
    A_DECRYPT_ERROR,            /* Decryption error */
    A_PHY_ERROR,                /* RX PHY error */
    A_CONSUMED,                  /* Object was consumed */
    A_CREDIT_UNAVAILABLE        /* Not enough credits to transmit packet */
} A_STATUS;

typedef void * HIF_HANDLE;
struct _NIC_DEV;

/**
 * @brief List of registration callbacks - filled in by HTC.
 */
struct htc_drvreg_callbacks {
    hif_status_t (* deviceInsertedHandler)(HIF_HANDLE hHIF, struct _NIC_DEV * os_hdl);
    hif_status_t (* deviceRemovedHandler)(void *instance);
};

/**
 * @brief List of callbacks - filled in by HTC.
 */ 
struct htc_callbacks {
    void *Context;  /**< context meaningful to HTC */
    hif_status_t (* txCompletionHandler)(void *Context, struct sk_buff *netbuf);
    hif_status_t (* rxCompletionHandler)(void *Context, struct sk_buff *netbuf, uint8_t pipeID);
    hif_status_t (* usbStopHandler)(void *Context, uint8_t status);
    hif_status_t (* usbDeviceRemovedHandler)(void *Context);
#ifdef MAGPIE_SINGLE_CPU_CASE
    void (*wlanTxCompletionHandler)(void *Context, uint8_t epnum);
#endif
};


typedef struct htc_drvreg_callbacks HTC_DRVREG_CALLBACKS;
typedef struct htc_callbacks HTC_CALLBACKS;

typedef struct _hif_device_usb {
    void *htc_handle;
    HTC_CALLBACKS htcCallbacks;

    struct _NIC_DEV * os_hdl;

} HIF_DEVICE_USB;


typedef struct UsbUrbContext
{
    u_int8_t        index;
    u_int8_t        inUse;
    u_int16_t       flags;
    struct sk_buff* buf; //adf_nbuf_t      buf;
    struct _NIC_DEV *osdev;
    urb_t           *urb;    
}UsbUrbContext_t;

typedef enum ath_hal_bus_type {
   HAL_BUS_TYPE_PCI,
   HAL_BUS_TYPE_AHB
} HAL_BUS_TYPE;


typedef void* HAL_BUS_TAG;
typedef void* HAL_BUS_HANDLE;

typedef struct hal_bus_info {
    HAL_BUS_TAG  bc_tag;
    int          cal_in_flash;
}HAL_BUS_INFO;


/*
 * Bus to hal context handoff
 */
typedef struct hal_bus_context {
    HAL_BUS_INFO     bc_info;
    HAL_BUS_HANDLE  bc_handle;
    HAL_BUS_TYPE    bc_bustype;
} HAL_BUS_CONTEXT;

typedef struct _NIC_DEV * osdev_t;

#define tq_struct tasklet_struct

typedef struct htc_tq_struct
{
    struct tq_struct tq;
    osdev_t osdev;
} htc_tq_struct_t;

typedef struct _UsbRxUrbContext {
    uint8_t        index;
    uint8_t        inUse;
    uint8_t        failcnt;
    struct sk_buff  *buf;
    struct _NIC_DEV *osdev;    
    urb_t           *WlanRxDataUrb;
    uint8_t       *rxUsbBuf;
} UsbRxUrbContext;


typedef struct UsbTxQ
{    
    struct sk_buff* buf; //adf_nbuf_t      buf;
//    a_uint8_t       hdr[80];
    uint16_t      hdrlen;
//    a_uint8_t       snap[8];
    uint16_t      snapLen;
//    a_uint8_t       tail[16];
    uint16_t      tailLen;
    uint16_t      offset;
    uint32_t      timeStamp;
} UsbTxQ_t;

typedef struct _UsbTxUrbContext {
    uint8_t         index;
    uint8_t         inUse;
    uint16_t        delay_free;
    struct sk_buff   *buf;
    struct _NIC_DEV  *osdev;
    void             *pipe;             
    urb_t            *urb; 
#if ZM_USB_TX_STREAM_MODE == 1    
    uint8_t        *txUsbBuf;
#endif        
} UsbTxUrbContext;


typedef struct _hif_usb_tx_pipe {
    uint8_t               TxPipeNum;
    UsbTxQ_t                UsbTxBufQ[ZM_MAX_TX_BUF_NUM];
    uint16_t              TxBufHead;
    uint16_t              TxBufTail;
    uint16_t              TxBufCnt;
    uint16_t              TxUrbHead;
    uint16_t              TxUrbTail;
    uint16_t              TxUrbCnt;
    UsbTxUrbContext         TxUrbCtx[ZM_MAX_TX_URB_NUM];
    usb_complete_t          TxUrbCompleteCb;
} HIFUSBTxPipe;


typedef void (*tasklet_callback_t)(void *ctx);
typedef void (*defer_func_t)(void *);


/* endpoint defines */
typedef enum
{
    ENDPOINT_UNUSED = -1,
    ENDPOINT0 = 0, /* this is reserved for the control endpoint */
    ENDPOINT1 = 1,  
    ENDPOINT2 = 2,   
    ENDPOINT3 = 3,
    ENDPOINT4,
    ENDPOINT5,
    ENDPOINT6,
    ENDPOINT7,
    ENDPOINT8,
    ENDPOINT_MAX = 22 /* maximum number of endpoints for this firmware build, max application
            endpoints = (ENDPOINT_MAX - 1) */
} HTC_ENDPOINT_ID;


typedef struct ath_usb_rx_info
{
    u_int16_t pkt_len;
    u_int16_t offset;
} ath_usb_rx_info_t;



struct _NIC_DEV {
    void                *bdev;      /* bus device handle */
    struct net_device   *netdev;    /* net device handle (wifi%d) */
    struct tq_struct    intr_tq;    /* tasklet */
    struct net_device_stats devstats;  /* net device statisitics */
	HAL_BUS_CONTEXT		bc;
#if !NO_SIMPLE_CONFIG
    uint32_t           sc_push_button_dur;
#endif

#ifdef ATH_USB
    struct usb_device       *udev;
    struct usb_interface    *interface;
#endif

#ifdef MAGPIE_HIF_PCI
    os_intr_func        func;
//    int                 irq;
#endif

#ifdef ATH_SUPPORT_HTC
    void                *wmi_dev;

    uint32_t   (*os_usb_submitMsgInUrb)(osdev_t osdev);
    uint32_t   (*os_usb_submitCmdOutUrb)(osdev_t osdev, uint8_t* buf, uint16_t len, void *context);
    uint32_t   (*os_usb_submitRxUrb)(osdev_t osdev);
    uint32_t   (*os_usb_submitTxUrb)(osdev_t osdev, uint8_t* buf, uint16_t len, void *context, HIFUSBTxPipe *pipe);
    uint16_t   (*os_usb_getFreeTxBufferCnt)(osdev_t osdev, HIFUSBTxPipe *pipe);
    uint16_t   (*os_usb_getMaxTxBufferCnt)(osdev_t osdev);
    uint16_t   (*os_usb_getTxBufferCnt)(osdev_t osdev, HIFUSBTxPipe *pipe);
    uint16_t   (*os_usb_getTxBufferThreshold)(osdev_t osdev);
    uint16_t   (*os_usb_getFreeCmdOutBufferCnt)(osdev_t osdev);
    uint16_t   (*os_usb_initTxRxQ)(osdev_t osdev);
    void       (*os_usb_enable_fwrcv)(osdev_t osdev);

    spinlock_t      cs_lock;
    spinlock_t      CmdUrbLock;

    void            *host_wmi_handle;
    void            *host_htc_handle;
    void            *host_hif_handle;

    /* Host HTC Endpoint IDs */
    HTC_ENDPOINT_ID         wmi_command_ep;
    HTC_ENDPOINT_ID         beacon_ep;
    HTC_ENDPOINT_ID         cab_ep;
    HTC_ENDPOINT_ID         uapsd_ep;
    HTC_ENDPOINT_ID         mgmt_ep;
    HTC_ENDPOINT_ID         data_VO_ep;
    HTC_ENDPOINT_ID         data_VI_ep;
    HTC_ENDPOINT_ID         data_BE_ep;
    HTC_ENDPOINT_ID         data_BK_ep;

//    adf_os_handle_t         sc_hdl;
    u_int32_t               rxepid;
    u_int8_t                target_vap_bitmap[4];
    u_int8_t                target_node_bitmap[32];
    
    htc_tq_struct_t         rx_tq;
    struct tq_struct        htctx_tq;
    struct tq_struct        htcuapsd_tq;
    // Magpie 
    u_int8_t                isMagpie;
    
    // Usb resource
    struct sk_buff*         regUsbReadBuf;
#define ZM_MAX_RX_URB_NUM                   2
    UsbRxUrbContext         RxUrbCtx[ZM_MAX_RX_URB_NUM];
    urb_t                   *RegInUrb;
    int                     RegInFailCnt;
    struct sk_buff*         UsbRxBufQ[ZM_MAX_RX_URB_NUM];
#define MAX_CMD_URB                         12   //4
    UsbUrbContext_t         UsbCmdOutCtxs[MAX_CMD_URB];
//	a_uint8_t                    txUsbBuf[ZM_MAX_TX_URB_NUM][ZM_USB_TX_BUF_SIZE];
//    urb_t                   *WlanTxDataUrb[ZM_MAX_TX_URB_NUM];
//    UsbTxQ_t                UsbTxBufQ[ZM_MAX_TX_BUF_NUM];
//    a_uint16_t              TxBufHead;
//    a_uint16_t              TxBufTail;
//    a_uint16_t              TxBufCnt;
//    a_uint16_t              TxUrbHead;
//    a_uint16_t              TxUrbTail;
//    a_uint16_t              TxUrbCnt;
    uint16_t              RxBufHead;
    uint16_t              RxBufTail;
    uint16_t              RxBufCnt;
//    UsbTxUrbContext         TxUrbCtx[ZM_MAX_TX_URB_NUM];    
    HIFUSBTxPipe            TxPipe;
    HIFUSBTxPipe            HPTxPipe;
//    HIFUSBTxPipe            MPTxPipe;
    
    struct sk_buff*         remain_buf;
    int                     remain_len;
    int                     check_pad;
    int                     check_len;

    atomic_t                txFrameNumPerSecond;
    atomic_t                txFrameCnt;
    struct timer_list       one_sec_timer;

    struct timer_list       tm_rxbuf_alloc;
    uint8_t               tm_rxbuf_act;

    void (*htcTimerHandler)(unsigned long arg);
    int (*htcAddTasklet)(osdev_t osdev, htc_tq_struct_t *, tasklet_callback_t, void *);
    int (*htcDelTasklet)(osdev_t osdev, htc_tq_struct_t *);
    int (*htcScheduleTasklet)(osdev_t osdev, htc_tq_struct_t *);
    int (*htcPutDeferItem)(osdev_t osdev, defer_func_t, int, void* ,void*, void*);
#if ZM_USB_STREAM_MODE
    unsigned int                   upstream_reg;
    uint32_t               upstream_reg_write_value;
#endif

    struct                  task_struct *athHTC_task;
    struct                  eventq *event_q;

#endif  /* #ifdef ATH_SUPPORT_HTC */

#ifdef ATH_USB
    unsigned long           event_flags;
    struct semaphore        recover_sem;
    struct work_struct      kevent;
    int                     enablFwRcv;
    u_int8_t                *Image;
    u_int32_t               ImageSize;
#endif

    int                     isModuleExit;
};
#endif	/* !_HIF_H_ */
