#ifndef _USB_API_H
#define _USB_API_H

#include <asm/types.h>
#include "usb_std.h"

struct usb_api {
	void (*_usb_init)(void);
	void (*_usb_init_stat)(void);
	void (*_usb_init_endpt)(void);
	void (*_usb_setup_descriptor)(void);
	void (*_usb_handle_interrupt)(void);
	void (*_usb_handle_vendor_request)(struct usb_ctrlrequest *ctrl);
	void (*_usb_handle_error)(__u32 status);
	void (*_usb_handle_setup)(void);
	void (*_usb_get_status)(struct usb_ctrlrequest *ctrl);
	void (*_usb_clear_feature)(struct usb_ctrlrequest *ctrl);
	void (*_usb_set_feature)(struct usb_ctrlrequest *ctrl);
	void (*_usb_set_address)(struct usb_ctrlrequest *ctrl);
	void (*_usb_get_descriptor)(struct usb_ctrlrequest *ctrl);
	void (*_usb_set_descriptor)(struct usb_ctrlrequest *ctrl);
	void (*_usb_get_configuration)(void);
	void (*_usb_set_configuration)(struct usb_ctrlrequest *ctrl);
	void (*_usb_get_interface)(struct usb_ctrlrequest *ctrl);
	void (*_usb_set_interface)(struct usb_ctrlrequest *ctrl);
	void (*_usb_read_usb_config)(void);
	int  (*_usb_read_otp_data)(__u8 *otp_ptr);
	void (*_usb_do_endpt0_rx)(int rx_total_bytes);
	void (*_usb_do_endpt0_tx)(int rx_total_bytes, __u8 *data);
	void (*_usb_jump_to_fw)(int firmware_addr, int firmware_size);
	void (*_usb_watchdog_reset_notify)();
	void (*_usb_init_by_starttype)();
	void (*_usb_config_phy_bias)();
	void (*_usb_init_cold_start)();
	void (*_usb_init_warm_start)();
	void (*_usb_init_watchdog_reset)();
};

void
usb_module_install(struct usb_api *api);

#endif
