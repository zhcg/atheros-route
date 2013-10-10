/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: upnp_wps_ctrlpt.h
//  Description: EAP-WPS UPnP control-point source header
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Sony Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************/

#ifndef UPNP_WPS_CTRLPT_H
#define UPNP_WPS_CTRLPT_H

struct upnp_wps_ctrlpt_sm;

struct upnp_wps_ctrlpt_ctx {
	int (*received_resp_get_device_info)(void *priv,
										 char *control_url,
										 u8 *msg, size_t msg_len);
	int (*received_resp_put_message)(void *priv,
									 char *control_url,
									 u8 *msg, size_t msg_len);

	int (*received_wlan_event)(void *priv,
							   char *control_url,
							   int ev_type, char *ev_mac,
							   u8 *msg, size_t msg_len);

#define UPNP_WPS_STATUS_CHANGE_CONFIGURATION		0x01
#define UPNP_WPS_STATUS_FAIL_AUTH_THRESOLD_REACHED	0x10
	int (*received_ap_status)(void *priv,
							  char *control_url, u8 status);
	int (*received_sta_status)(void *priv,
							   char *control_url, u8 status);
};

struct upnp_wps_ctrlpt_sm *
upnp_wps_ctrlpt_init(struct upnp_wps_ctrlpt_ctx *ctx, void *priv);
void upnp_wps_ctrlpt_deinit(struct upnp_wps_ctrlpt_sm *sm);

int upnp_wps_ctrlpt_start(struct upnp_wps_ctrlpt_sm *sm, char *net_if);
int upnp_wps_ctrlpt_stop(struct upnp_wps_ctrlpt_sm *sm);

#ifndef NAME_SIZE
#define NAME_SIZE 256
#endif /* NAME_SIZE */

struct upnp_wps_ctrlpt_device {
	char manufacturer[250];
	char model_name[250];
	char model_number[250];
	char serial_number[250];
	char udn[250];
	char control_url[NAME_SIZE];
};

struct upnp_wps_ctrlpt_device_list {
	struct upnp_wps_ctrlpt_device device;
	struct upnp_wps_ctrlpt_device_list *next;
};

int upnp_wps_ctrlpt_get_scan_results(struct upnp_wps_ctrlpt_sm *sm,
								struct upnp_wps_ctrlpt_device_list **list);
void upnp_wps_ctrlpt_destroy_device_list(struct upnp_wps_ctrlpt_device_list *list);

int upnp_wps_ctrlpt_refresh_device(struct upnp_wps_ctrlpt_sm *sm, int timeout);
int upnp_wps_ctrlpt_send_get_device_info(struct upnp_wps_ctrlpt_sm *sm,
										 char *control_url);
int upnp_wps_ctrlpt_send_put_message(struct upnp_wps_ctrlpt_sm *sm,
									 char *control_url,
									 u8 *msg, size_t msg_len);
#define UPNP_WPS_WLANEVENT_TYPE_PROBE	1
#define UPNP_WPS_WLANEVENT_TYPE_EAP		2
int upnp_wps_ctrlpt_send_put_wlan_response(struct upnp_wps_ctrlpt_sm *sm,
										   char *control_url, int ev_type,
#ifdef CONFIG_NATIVE_WINDOWS
										   u8 *mac,
#endif /* CONFIG_NATIVE_WINDOWS */
										   u8 *msg, size_t msg_len);
int upnp_wps_ctrlpt_send_set_selected_registrar(struct upnp_wps_ctrlpt_sm *sm,
												char *control_url,
												u8 *msg, size_t msg_len);
#endif /* UPNP_WPS_CTRLPT_H */
