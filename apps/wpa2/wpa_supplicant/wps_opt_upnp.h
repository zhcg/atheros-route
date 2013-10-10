/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wps_opt_upnp.h
//  Description: EAP-WPS UPnP option source header
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

#ifndef WPS_OPT_UPNP_H
#define WPS_OPT_UPNP_H

struct wpa_config;
struct wps_opt_upnp_sm;
struct upnp_wps_ctrlpt_device_list;

struct wps_opt_upnp_sm_ctx {
	void *ctx;		/* pointer to arbitrary upper level context */
	void *msg_ctx;

	struct wpa_config *(*get_conf)(void *ctx);
};

struct wps_opt_upnp_sm *wps_opt_upnp_sm_init(struct wps_opt_upnp_sm_ctx *ctx);
void wps_opt_upnp_sm_deinit(struct wps_opt_upnp_sm *sm);

int wps_opt_upnp_sm_start(struct wps_opt_upnp_sm *sm, char *net_if);
int wps_opt_upnp_sm_stop(struct wps_opt_upnp_sm *sm);

int wps_opt_upnp_refresh_device(struct wps_opt_upnp_sm *sm, int timeout);
int wps_opt_upnp_get_scan_result(struct wps_opt_upnp_sm *sm,
								 struct upnp_wps_ctrlpt_device_list **list);
int wps_opt_upnp_get_device_info(struct wps_opt_upnp_sm *sm,
								 char *control_url);
int wps_opt_upnp_set_selected_registrar(struct wps_opt_upnp_sm *sm,
										char *control_url,
										int selected);
#endif /* WPS_OPT_UPNP_H */
