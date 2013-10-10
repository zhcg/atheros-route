/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wps_opt_upnp.c
//  Description: EAP-WPS UPnP option source
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

#include "includes.h"

#ifndef CONFIG_NATIVE_WINDOWS
#include <upnp/ithread.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "defs.h"
#include "common.h"
#include "wpa_supplicant.h"
#include "wpa.h"
#include "eloop.h"
#include "config.h"
#include "wps_config.h"
#include "wpa_ctrl.h"
#include "state_machine.h"
#include "wps_parser.h"
#include "wps_opt_upnp.h"
#include "eap_wps.h"
#include "upnp_wps_ctrlpt.h"

#define STATE_MACHINE_DATA struct wps_opt_upnp_sm
#define STATE_MACHINE_DEBUG_PREFIX "OPT_UPNP"

#ifdef CONFIG_NATIVE_WINDOWS
typedef HANDLE mutex_t;
#else /* CONFIG_NATIVE_WINDOWS */
typedef ithread_mutex_t mutex_t;
#endif /* CONFIG_NATIVE_WINDOWS */


/**
 * struct wps_opt_upnp_sm - Internal data for UPNP state machines
 */

typedef enum {
	OPT_UPNP_INACTIVE = 0,
	OPT_UPNP_SCANNING,
	OPT_UPNP_SCAN_TIMEOUT
} opt_upnp_states;

struct opt_upnp_device_info {
	char *control_url;
	int event_type;
	char event_mac[18];
	struct eap_wps_data *data;
};

struct opt_upnp_device_node {
	struct opt_upnp_device_info info;
	struct opt_upnp_device_node *next;
};

struct wps_opt_upnp_sm {
	opt_upnp_states OPT_UPNP_state;
	Boolean changed;
	struct wps_opt_upnp_sm_ctx *ctx;
	struct upnp_wps_ctrlpt_sm *upnp_ctrlpt_sm;
	struct os_time scanTimeout;
	mutex_t mutex_devlist;
	struct opt_upnp_device_node *list;
};


static void wps_opt_upnp_request(struct wps_opt_upnp_sm *sm,
								 int req_type, const char *msg,
								 size_t msg_len)
{
#define CTRL_REQ_TYPE_COMP			0
#define CTRL_REQ_TYPE_FAIL			1
#define CTRL_REQ_TYPE_PASSWORD		2
	char *buf;
	size_t buflen;
	int len = 0;
	char *field;
	char *txt;

	if (!sm || !sm->ctx)
		return;

	switch(req_type) {
	case CTRL_REQ_TYPE_COMP:
		field = "UPNP_COMP";
		txt = "Complete EAP-WPS with UPnP authentication";
		break;
	case CTRL_REQ_TYPE_FAIL:
		field = "UPNP_FAIL";
		txt = "Fail EAP-WPS with UPnP authentication";
		break;
	case CTRL_REQ_TYPE_PASSWORD:
		field = "UPNP_PASSWORD";
		txt = "Request Password for EAP-WPS with UPnP";
		break;
	default:
		return;
	}

	buflen = 100 + os_strlen(txt);
	buf = os_malloc(buflen);
	if (buf == NULL)
		return;
	len = os_snprintf(buf + len, buflen - len, WPA_CTRL_REQ "%s%s%s%s-%s ",
		       field, msg?":[":"", msg?msg:"", msg?"]":"", txt);
	if (len < 0 || (size_t) len >= buflen) {
		free(buf);
		return;
	}
	buf[buflen - 1] = '\0';
	wpa_msg(sm->ctx->msg_ctx, MSG_INFO, "%s", buf);
	free(buf);
}


int
wps_opt_upnp_refresh_device(struct wps_opt_upnp_sm *sm, int timeout)
{
	int ret = -1;

	do {
		if (!sm)
			break;

		if (upnp_wps_ctrlpt_refresh_device(sm->upnp_ctrlpt_sm, timeout))
			break;

		ret = 0;
	} while (0);

	return ret;
}


int
wps_opt_upnp_get_scan_result(struct wps_opt_upnp_sm *sm,
							 struct upnp_wps_ctrlpt_device_list **list)
{
	int ret = -1;

	do {
		if (!sm || !list)
			break;
		*list = 0;

		if (upnp_wps_ctrlpt_get_scan_results(sm->upnp_ctrlpt_sm, list))
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (list && *list) {
			upnp_wps_ctrlpt_destroy_device_list(*list);
			*list = 0;
		}
	}

	return ret;
}


static void wps_opt_upnp_deinit_data(struct eap_wps_data *data)
{
	do {
		if (!data)
			break;
		eap_wps_config_deinit_data(data);
	} while (0);
}


static int wps_opt_upnp_init_data(struct wps_opt_upnp_sm *sm,
								  struct eap_wps_data **data)
{
	int ret = -1;
	struct wps_config *conf;

	do {
		if (!sm || !sm->ctx || !data)
			break;
		*data = 0;

		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   (sm->ctx->get_conf(sm->ctx->ctx))->wps:0;
		if (!conf)
			break;

		*data = (struct eap_wps_data *)wpa_zalloc(sizeof(**data));
		if (!*data)
			break;
		if (eap_wps_config_init_data(conf, *data))
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (data && *data) {
			free(*data);
			*data = 0;
		}
	}

	return ret;
}


static int wps_opt_upnp_get_device_node(struct wps_opt_upnp_sm *sm,
										char *control_url,
										struct opt_upnp_device_node **node)
{
	int ret = -1;
	struct opt_upnp_device_node *cur;

	do {
		if (!sm || !control_url || !node)
			break;

		*node = 0;
		cur = sm->list;
		while (cur) {
			if (cur->info.control_url &&
				!os_strcmp(cur->info.control_url, control_url)) {
				*node = cur;
				ret = 0;
				break;
			}
			cur = cur->next;
		}
	} while (0);
	if (ret && node && *node)
		*node = 0;
	return ret;
}


static void wps_opt_upnp_del_device_node(struct opt_upnp_device_node *node)
{
	do {
		if (!node)
			break;
		if (node->info.control_url)
			free(node->info.control_url);
		wps_opt_upnp_deinit_data(node->info.data);
		free(node);
		node = 0;
	} while (0);
}


static int wps_opt_upnp_remove_device_node(struct wps_opt_upnp_sm *sm,
										   char *control_url)
{
	int ret = -1;
	struct opt_upnp_device_node *prev = 0, *cur;

	do {
		if (!sm || !control_url)
			break;

		cur = sm->list;
		while (cur) {
			if (cur->info.control_url &&
				!os_strcmp(cur->info.control_url, control_url)) {
				if (cur == sm->list)
					sm->list = cur->next;
				else if (prev)
					prev->next = cur->next;
				else
					break;
				wps_opt_upnp_del_device_node(cur);
				ret = 0;
				break;
			}
			prev = cur;
			cur = cur->next;
		}
	} while (0);

	return ret;
}


static void wps_opt_upnp_remove_all_device_node(struct wps_opt_upnp_sm *sm)
{
	struct opt_upnp_device_node *cur, *next;

	do {
		if (!sm)
			break;

		cur = sm->list;
		while (cur) {
			next = cur->next;
			wps_opt_upnp_del_device_node(cur);
			cur = next;
		}
		sm->list = 0;
	} while (0);
}


static struct opt_upnp_device_node *
wps_opt_upnp_add_device_node(struct wps_opt_upnp_sm *sm,
							 char *control_url)
{
	int ret = -1;
	struct opt_upnp_device_node *node = 0, *next;

	do {
		if (!sm || !control_url)
			break;

		node = sm->list;
		while (node) {
			if (node->info.control_url &&
				!os_strcmp(node->info.control_url, control_url))
				break;
			node = node->next;
		}

		if (node) {
			wps_opt_upnp_deinit_data(node->info.data);
			if (wps_opt_upnp_init_data(sm, &node->info.data))
				break;
		} else {
			node = (struct opt_upnp_device_node *)wpa_zalloc(sizeof(*node));
			if (!node)
				break;
			node->info.control_url =
				(char *)wpa_zalloc(os_strlen(control_url) + 1);
			if (!node->info.control_url)
				break;
			strcpy(node->info.control_url, control_url);
			node->info.data =
				(struct eap_wps_data *)wpa_zalloc(sizeof(*node->info.data));
			if (!node->info.data)
				break;
			if (wps_opt_upnp_init_data(sm, &node->info.data))
				break;

			if (sm->list) {
				next = sm->list;
				while (next->next)
					next = next->next;
				next->next = node;
			} else
				sm->list = node;
		}

		ret = 0;
	} while (0);

	if (ret) {
		if (node) {
			(void)wps_opt_upnp_remove_device_node(sm, control_url);
			wps_opt_upnp_del_device_node(node);
			node = 0;
		}
	}
	return node;
}


int
wps_opt_upnp_select_ssid_configuration(struct wps_opt_upnp_sm *sm,
									   char *control_url,
									   struct wps_config *conf,
									   struct eap_wps_data *data,
									   u8 *raw_data, size_t raw_data_len,
									   Boolean wrap_credential)
{
	int ret = -1;
	int index;
	char msg[BUFSIZ];

	do {
		if (!sm || !conf || !data || !raw_data || !raw_data_len)
			break;

		index = eap_wps_config_select_ssid_configuration(conf, data,
														 raw_data, raw_data_len,
														 wrap_credential);
		if (0 > index)
			break;

		os_snprintf(msg, sizeof(msg), "%d:%s", index, control_url);
		wps_opt_upnp_request(sm, CTRL_REQ_TYPE_COMP, msg, os_strlen(msg));

		ret = 0;
	} while (0);

	return ret;
}


static u8 *
wps_opt_upnp_build_req_registrar(struct wps_opt_upnp_sm *sm,
								 char *control_url,
								 struct wps_config *conf,
								 struct eap_wps_data *data,
								 size_t *rsp_len)
{
	u8 *rsp = 0;
	struct eap_wps_target_info *target;

	do {
		if (!sm || !conf || !data || !data->target || !rsp_len)
			break;
		*rsp_len = 0;

		target = data->target;

		switch (data->state) {
		case M2:
		{
			/* Build M2 message */
			rsp = eap_wps_config_build_message_M2(conf, data, rsp_len);
			if (!rsp)
				break;
			/* Should be received M3 message */
			data->state = M3;
			break;
		}
		case M2D:
		{
			char msg[BUFSIZ];

			/* Build M2D message */
			rsp = eap_wps_config_build_message_M2D(conf, data, rsp_len);
			if (!rsp)
				break;

			switch (data->reg_mode) {
			case WPS_SUPPLICANT_REGMODE_REGISTER_STA:
				os_snprintf(msg, sizeof(msg), "REGISTRAR_STA:%s", control_url);
				break;
			default:
				msg[0] = 0;
				break;
			}
			if (msg[0])
				wps_opt_upnp_request(sm, CTRL_REQ_TYPE_PASSWORD, msg, os_strlen(msg));

			/* wait for receiving DONE message or inputting PIN */
			data->state = DONE;
			break;
		}
		case M4:
		{
			/* Build M4 message */
			rsp = eap_wps_config_build_message_M4(conf, data, rsp_len);
			if(!rsp)
				break;
			/* Should be received M5 message */
			data->state = M5;
			break;
		}
		case M6:
		{
			/* Build M6 message */
			rsp = eap_wps_config_build_message_M6(conf, data, rsp_len);
			if(!rsp)
				break;
			/* Should be received M7 message */
			data->state = M7;
			break;
		}
		case M8:
		{
			if (data->reg_mode == WPS_SUPPLICANT_REGMODE_REGISTER_STA) {
				int res = -1;
				struct wps_data *wps = 0;
				u8 *tmp;
				size_t tmp_len;
				do {
					if (wps_create_wps_data(&wps))
						break;

					if (wps_set_value(wps, WPS_TYPE_CREDENTIAL,
									  data->config, data->config_len))
						break;

					if (wps_write_wps_data(wps, &tmp, &tmp_len))
						break;

					res = 0;
				} while (0);
				(void)wps_destroy_wps_data(&wps);

				if (!res) {
					free(data->config);
					data->config = tmp;
					data->config_len = tmp_len;
				} else {
					if (data->config) free(data->config);
					data->config = 0;
					data->config_len = 0;
					if (tmp) free(tmp);
					break;
				}
			}

			/* Build M8 message */
			rsp = eap_wps_config_build_message_M8(conf, data, rsp_len);
			if(!rsp)
				break;
			/* Should be received Done message */
			data->state = DONE;
			break;
		}
		case NACK:
		{

			char msg[BUFSIZ];

			rsp = eap_wps_config_build_message_special(conf, data,
													   WPS_MSGTYPE_NACK,
													   target->nonce,
													   data->nonce,
													   rsp_len);
			if (conf->reg_mode == WPS_SUPPLICANT_REGMODE_CONFIGURE_AP) {
				if (conf->dev_pwd_len) {
					conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
					os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
					conf->dev_pwd_len = 0;
				}

				if (conf->set_pub_key) {
					if (conf->dh_secret)
						eap_wps_free_dh(&conf->dh_secret);
					os_memset(conf->pub_key, 0, sizeof(conf->pub_key));
					conf->set_pub_key = 0;
				}

				if (data->preset_pubKey) {
					data->dh_secret = 0;
					os_memset(data->pubKey, 0, sizeof(data->pubKey));
					data->preset_pubKey = 0;
				}

				data->state = FAILURE;

				/* Send EAP-WPS fail message */
				os_snprintf(msg, sizeof(msg), "%d:%s", 0, control_url);
				wps_opt_upnp_request(sm, CTRL_REQ_TYPE_FAIL, msg, os_strlen(msg));
			} else {
				data->state = NACK;
			}
		}
		default:
		{
			break;
		}
		}
	} while (0);

	if (!rsp && rsp_len)
		*rsp_len = 0;

	return rsp;
}


static int
wps_opt_upnp_process_registrar(struct wps_opt_upnp_sm *sm,
							   char *control_url,
							   struct wps_config *conf,
							   struct eap_wps_data *data,
							   u8 *req, size_t req_len)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	int prev_state;
        struct wpa_supplicant *wpa_s = sm->ctx->msg_ctx;     /* %% right? %% */

	do {
		if (!sm || !conf || !data || !data->target || !req)
			break;
		target = data->target;

		if (data->rcvMsg) {
			free(data->rcvMsg);
			data->rcvMsg = 0;
			data->rcvMsgLen = 0;
		}

		if (!req || !req_len)
			break;

		data->rcvMsg = (u8 *)wpa_zalloc(req_len);
		if (!data->rcvMsg)
			break;
		os_memcpy(data->rcvMsg, req, req_len);
		data->rcvMsgLen = req_len;

		prev_state = data->state;
		switch (data->state) {
		case START:
		{
			/* Should be received M1 message */
			if (!eap_wps_config_process_message_M1(conf, data)) {
				if (data->dev_pwd_len) {
					/* Build M2 message */
					data->state = M2;
				} else {
					/* Build M2D message */
					data->state = M2D;
				}
			} else
				data->state = NACK;
			break;
		}
		case M3:
		{
			/* Should be received M3 message */
			if (!eap_wps_config_process_message_M3(conf, data)) {
				/* Build M4 message */
				data->state = M4;
			} else
				data->state = NACK;
			break;
		}
		case M5:
		{
			/* Should be received M5 message */
			if (!eap_wps_config_process_message_M5(conf, data)) {
				/* Build M6 message */
				data->state = M6;
			} else
				data->state = NACK;
			break;
		}
		case M7:
		{
			/* Should be received M7 message */
			if (!eap_wps_config_process_message_M7(conf, data)) {
				/* Build M8 message */
				data->state = M8;
			} else
				data->state = NACK;
			break;
		}
		case DONE:
		{
			/* Should be received Done */
			if (!eap_wps_config_process_message_special(conf, data, WPS_MSGTYPE_DONE, target->nonce, data->nonce)) {
				switch (data->reg_mode) {
				case WPS_SUPPLICANT_REGMODE_CONFIGURE_AP:
					/* Select Network Configuration (already added) */
					(void)wps_opt_upnp_select_ssid_configuration(
										sm, control_url, conf, data,
										data->config, data->config_len, 0);
					break;
				case WPS_SUPPLICANT_REGMODE_REGISTER_STA:
					/* Select Network Configuration (already added) */
					(void)wps_opt_upnp_select_ssid_configuration(
										sm, control_url, conf, data,
										data->config, data->config_len, 1);
					break;
				default:
					break;
				}

				if (conf->dev_pwd_len) {
					conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
					os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
					conf->dev_pwd_len = 0;
				}

				if (conf->set_pub_key) {
					if (conf->dh_secret)
						eap_wps_free_dh(&conf->dh_secret);
					os_memset(conf->pub_key, 0, sizeof(conf->pub_key));
					conf->set_pub_key = 0;
				}

				if (data->preset_pubKey) {
					data->dh_secret = 0;
					os_memset(data->pubKey, 0, sizeof(data->pubKey));
					data->preset_pubKey = 0;
				}
				data->state = FAILURE;

                                /* All done (?). Disable WPS mode now,
                                 * including killing off the temporary network
                                 * description we were using.
                                 * The original Sony code did not ever
                                 * seem to disable WPS when done... !
                                 */
                                (void) eap_wps_disable(wpa_s, conf);
			} else if (WPS_MSGTYPE_DONE == wps_get_message_type(data->rcvMsg, data->rcvMsgLen)) {
				data->state = FAILURE;
			} else if (!eap_wps_config_process_message_M1(conf, data)) {
				if (conf->dev_pwd_len)
					data->state = M2;
			} else
				data->state = NACK;
			break;
		}
		case NACK:
		{
			char msg[BUFSIZ];

			/* Should be received NACK */
			(void)eap_wps_config_process_message_special(conf, data,
														 WPS_MSGTYPE_NACK,
														 target->nonce,
														 data->nonce);
			if (conf->reg_mode == WPS_SUPPLICANT_REGMODE_CONFIGURE_AP) {
				data->state = NACK;
			} else {
				if (conf->dev_pwd_len) {
					conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
					os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
					conf->dev_pwd_len = 0;
				}

				if (conf->set_pub_key) {
					if (conf->dh_secret)
						eap_wps_free_dh(&conf->dh_secret);
					os_memset(conf->pub_key, 0, sizeof(conf->pub_key));
					conf->set_pub_key = 0;
				}

				if (data->preset_pubKey) {
					data->dh_secret = 0;
					os_memset(data->pubKey, 0, sizeof(data->pubKey));
					data->preset_pubKey = 0;
				}

				data->state = FAILURE;

				/* Send EAP-WPS fail message */
				os_snprintf(msg, sizeof(msg), "%d:%s", 0, control_url);
				wps_opt_upnp_request(sm, CTRL_REQ_TYPE_FAIL, msg, os_strlen(msg));
			}

			break;
		}
		default:
		{
			break;
		}
		}
		if (((data->state != DONE) && (data->state != NACK))
			 && (prev_state == data->state))
			break;
		ret = 0;
	} while (0);

	return ret;
}


int
wps_opt_upnp_get_device_info(struct wps_opt_upnp_sm *sm,
							 char *control_url)
{
	int ret = -1;

	do {
		if (!sm || !control_url)
			break;

		if (upnp_wps_ctrlpt_send_get_device_info(sm->upnp_ctrlpt_sm,
												 control_url))
			break;

		ret = 0;
	} while (0);

	return ret;
}


int
wps_opt_upnp_set_selected_registrar(struct wps_opt_upnp_sm *sm,
									char *control_url,
									int selected)
{
	int ret = -1;
	struct wpa_config *conf;
	struct wps_config *wps;
	struct wps_data *data = 0;
	Boolean blval;
	u8 u8val;
	u16 u16val;
	u8 *msg = 0;
	size_t msg_len;

	do {
		if (!sm || !sm->ctx)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx);
		if (!conf || !conf->wps)
			break;
		wps = conf->wps;

		if (wps_create_wps_data(&data))
			break;

		u8val = wps->version;
		if (!u8val)
			u8val = WPS_VERSION;
		if (wps_set_value(data, WPS_TYPE_VERSION, &u8val, 0))
			break;

		/* Selected Registrar */
		blval = selected?1:0;
		if (wps_set_value(data, WPS_TYPE_SEL_REGISTRAR, &blval, 0))
			break;

		/* Device Password ID */
		u16val = selected?wps->dev_pwd_id:WPS_DEVICEPWDID_DEFAULT;
		if (wps_set_value(data, WPS_TYPE_DEVICE_PWD_ID, &u16val, 0))
			break;

		/* Selected Registrar Config Methods */
		u16val = selected?wps->config_methods:0;
		if (wps_set_value(data, WPS_TYPE_SEL_REG_CFG_METHODS, &u16val, 0))
			break;

		if (wps_write_wps_data(data, &msg, &msg_len))
			break;

		if (upnp_wps_ctrlpt_send_set_selected_registrar(sm->upnp_ctrlpt_sm,
														control_url,
														msg, msg_len))
			break;

		ret = 0;
	} while (0);

	if (msg)
		free(msg);
	(void)wps_destroy_wps_data(&data);

	return ret;
}


static void
wps_opt_upnp_build_req_timer(void *priv, void *control_url)
{
	struct wps_opt_upnp_sm *sm;
	struct opt_upnp_device_node *node;
	struct wps_config *conf;
	struct eap_wps_data *data;
	u8 *rsp = 0;
	size_t rsp_len;

	do {
		if (!priv || !control_url)
			break;

		sm = priv;
		if (!sm->ctx)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   (sm->ctx->get_conf(sm->ctx->ctx))->wps:0;
		if (!conf)
			break;

#ifdef CONFIG_NATIVE_WINDOWS
		WaitForSingleObject(sm->mutex_devlist, INFINITE);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_lock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */

		do {
			if (wps_opt_upnp_get_device_node(sm, control_url, &node)) {
				break;
			}

			data = node->info.data;
			if (!data)
				break;

			rsp = wps_opt_upnp_build_req_registrar(sm, control_url,
												   conf, data,
												   &rsp_len);
		} while (0);

#ifdef CONFIG_NATIVE_WINDOWS
		ReleaseMutex(sm->mutex_devlist);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_unlock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */

		if (rsp) {
			switch (data->reg_mode) {
			case WPS_SUPPLICANT_REGMODE_CONFIGURE_AP:
				if (upnp_wps_ctrlpt_send_put_message(sm->upnp_ctrlpt_sm,
													 control_url,
													 rsp, rsp_len))
					break;
				break;
			case WPS_SUPPLICANT_REGMODE_REGISTER_STA:
				if (upnp_wps_ctrlpt_send_put_wlan_response(sm->upnp_ctrlpt_sm,
													 control_url,
													 UPNP_WPS_WLANEVENT_TYPE_EAP,
#ifdef CONFIG_NATIVE_WINDOWS
													 conf->mac,
#endif /* CONFIG_NATIVE_WINDOWS */
													 rsp, rsp_len))
					break;
				break;
			default:
				break;
			}
		}
	} while (0);

	if (rsp)
		free(rsp);
	if (control_url)
		free(control_url);
}


static int
wps_opt_upnp_received_resp_get_device_info(void *priv,
										   char *control_url,
										   u8 *msg, size_t msg_len)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm;
	struct opt_upnp_device_node *node;
	struct wps_config *conf;
	struct eap_wps_data *data;
	char *ctrl_url = 0;

	do {
		if (!priv || !control_url || !msg)
			break;

		sm = priv;
		if (!sm->ctx)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   (sm->ctx->get_conf(sm->ctx->ctx))->wps:0;
		if (!conf)
			break;

#ifdef CONFIG_NATIVE_WINDOWS
		WaitForSingleObject(sm->mutex_devlist, INFINITE);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_lock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */

		do {
			conf->reg_mode = WPS_SUPPLICANT_REGMODE_CONFIGURE_AP;
			node = wps_opt_upnp_add_device_node(sm, control_url);
			if (!node)
				break;

			data = node->info.data;
			if (!data)
				break;

			if (wps_opt_upnp_process_registrar(sm, control_url,
											   conf, data,
											   msg, msg_len)) {
				wps_opt_upnp_remove_device_node(sm, control_url);
				break;
			}

			if (FAILURE == data->state)
				wps_opt_upnp_remove_device_node(sm, control_url);
			else {
				ctrl_url = os_strdup(control_url);
				if (!ctrl_url)
					break;
				eloop_register_timeout(0, 0, wps_opt_upnp_build_req_timer, sm, ctrl_url);
			}
			ret = 0;
		} while (0);

#ifdef CONFIG_NATIVE_WINDOWS
		ReleaseMutex(sm->mutex_devlist);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_unlock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */
	} while (0);

	if (ret && ctrl_url)
		free(ctrl_url);

	return ret;
}


static int
wps_opt_upnp_received_resp_put_message(void *priv,
									   char *control_url,
									   u8 *msg, size_t msg_len)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm;
	struct opt_upnp_device_node *node;
	struct wps_config *conf;
	struct eap_wps_data *data;
	char *ctrl_url = 0;

	do {
		if (!priv || !control_url || !msg)
			break;

		sm = priv;
		if (!sm->ctx)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   (sm->ctx->get_conf(sm->ctx->ctx))->wps:0;
		if (!conf)
			break;

#ifdef CONFIG_NATIVE_WINDOWS
		WaitForSingleObject(sm->mutex_devlist, INFINITE);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_lock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */

		do {
			if (wps_opt_upnp_get_device_node(sm, control_url, &node))
				break;

			data = node->info.data;
			if (!data)
				break;

			if (wps_opt_upnp_process_registrar(sm, control_url,
											   conf, data,
											   msg, msg_len)) {
				wps_opt_upnp_remove_device_node(sm, control_url);
				break;
			}

			if (FAILURE == data->state)
				wps_opt_upnp_remove_device_node(sm, control_url);
			else {
				ctrl_url = os_strdup(control_url);
				if (!ctrl_url)
					break;
				eloop_register_timeout(0, 0, wps_opt_upnp_build_req_timer, sm, ctrl_url);
			}
			ret = 0;
		} while (0);

#ifdef CONFIG_NATIVE_WINDOWS
		ReleaseMutex(sm->mutex_devlist);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_unlock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */
	} while (0);

	if (ret && ctrl_url)
		free(ctrl_url);

	return ret;
}


static int
wps_opt_upnp_received_wlan_event(void *priv,
								 char *control_url,
								 int event_type,
								 char *event_mac,
								 u8 *msg, size_t msg_len)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm;
	struct opt_upnp_device_node *node;
	struct wps_config *conf;
	struct eap_wps_data *data;
	char *ctrl_url = 0;

	do {
		if (!priv || !control_url || !msg)
			break;

		sm = priv;
		if (!sm->ctx)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   (sm->ctx->get_conf(sm->ctx->ctx))->wps:0;
		if (!conf)
			break;

#ifdef CONFIG_NATIVE_WINDOWS
		WaitForSingleObject(sm->mutex_devlist, INFINITE);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_lock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */

		do {
			if (WPS_MSGTYPE_M1 == wps_get_message_type(msg, msg_len)) {
				int state = START;
				conf->reg_mode = WPS_SUPPLICANT_REGMODE_REGISTER_STA;
				do {
					if (wps_opt_upnp_get_device_node(sm, control_url, &node))
						break;

					data = node->info.data;
					if (!data)
						break;

					if (DONE != data->state)
						break;

					state = DONE;
				} while (0);

				node = wps_opt_upnp_add_device_node(sm, control_url);
				if (!node)
					break;

				data = node->info.data;
				if (!data)
					break;

				data->state = state;
			} else if (wps_opt_upnp_get_device_node(sm, control_url, &node))
				break;

			data = node->info.data;
			if (!data)
				break;

			node->info.event_type = event_type;
			os_snprintf(node->info.event_mac, sizeof(node->info.event_mac), "%s", event_mac);

			if (wps_opt_upnp_process_registrar(sm, control_url,
											   conf, data,
											   msg, msg_len)) {
				wps_opt_upnp_remove_device_node(sm, control_url);
				break;
			}

			if (FAILURE == data->state) {
				if ((conf->cur_upnp_device) &&
					!os_strcmp(control_url, conf->cur_upnp_device)) {
					free(conf->cur_upnp_device);
					conf->cur_upnp_device = 0;
				}
				wps_opt_upnp_remove_device_node(sm, control_url);
			} else {
				ctrl_url = os_strdup(control_url);
				if (!ctrl_url)
					break;
				eloop_register_timeout(0, 0, wps_opt_upnp_build_req_timer, sm, ctrl_url);
			}
			ret = 0;
		} while (0);

#ifdef CONFIG_NATIVE_WINDOWS
		ReleaseMutex(sm->mutex_devlist);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_unlock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */
	} while (0);

	if (ret && ctrl_url)
		free(ctrl_url);

	return ret;
}


static int
wps_opt_upnp_received_ap_status(void *priv,
								char *control_url, u8 status)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm;

	do {
		if (!priv || !control_url)
			break;

		sm = priv;

		ret = 0;
	} while (0);
	return ret;
}


static int
wps_opt_upnp_received_sta_status(void *priv,
								 char *control_url, u8 status)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm;

	do {
		if (!priv || !control_url)
			break;

		sm = priv;

		ret = 0;
	} while (0);
	return ret;
}


struct wps_opt_upnp_sm *
wps_opt_upnp_sm_init(struct wps_opt_upnp_sm_ctx *ctx)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm = 0;
	struct upnp_wps_ctrlpt_ctx *cp_ctx = 0;
	struct upnp_wps_ctrlpt_sm *cp = 0;
	struct wps_config *conf;

	do {
		cp_ctx = wpa_zalloc(sizeof(*cp_ctx));
		if (!cp_ctx)
			break;
		cp_ctx->received_resp_get_device_info = 
			wps_opt_upnp_received_resp_get_device_info;
		cp_ctx->received_resp_put_message = 
			wps_opt_upnp_received_resp_put_message;
		cp_ctx->received_wlan_event = 
			wps_opt_upnp_received_wlan_event;
		cp_ctx->received_ap_status = 
			wps_opt_upnp_received_ap_status;
		cp_ctx->received_sta_status = 
			wps_opt_upnp_received_sta_status;

		sm = wpa_zalloc(sizeof(*sm));
		if (!sm)
			break;
		sm->ctx = ctx;
#ifdef CONFIG_NATIVE_WINDOWS
		sm->mutex_devlist = CreateMutex(0, 0, 0);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_init(&sm->mutex_devlist, 0);
#endif /* CONFIG_NATIVE_WINDOWS */

		cp = upnp_wps_ctrlpt_init(cp_ctx, sm);
		if (!cp)
			break;
		sm->upnp_ctrlpt_sm = cp;

		if (!sm->ctx->get_conf)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx)?sm->ctx->get_conf(sm->ctx->ctx)->wps:0;
		if (!conf)
			break;

		if (conf->upnp_iface) {
			if (upnp_wps_ctrlpt_start(sm->upnp_ctrlpt_sm, conf->upnp_iface))
				break;
		}

		ret = 0;
	} while (0);

	if (ret) {
		if (sm) {
			wps_opt_upnp_sm_deinit(sm);
			sm = 0;
		} else if (cp)
			upnp_wps_ctrlpt_deinit(cp);
		else if (cp_ctx)
			free(cp_ctx);
	}

	return sm;
}


void wps_opt_upnp_sm_deinit(struct wps_opt_upnp_sm *sm)
{
	do {
		if (!sm)
			break;

#ifdef CONFIG_NATIVE_WINDOWS
		WaitForSingleObject(sm->mutex_devlist, INFINITE);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_lock(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */

		wps_opt_upnp_remove_all_device_node(sm);
		upnp_wps_ctrlpt_deinit(sm->upnp_ctrlpt_sm);

#ifdef CONFIG_NATIVE_WINDOWS
		ReleaseMutex(sm->mutex_devlist);
		CloseHandle(sm->mutex_devlist);
#else /* CONFIG_NATIVE_WINDOWS */
		ithread_mutex_unlock(&sm->mutex_devlist);
		ithread_mutex_destroy(&sm->mutex_devlist);
#endif /* CONFIG_NATIVE_WINDOWS */

		free(sm->ctx);
		free(sm);
	} while (0);
}


int wps_opt_upnp_sm_start(struct wps_opt_upnp_sm *sm, char *net_if)
{
	int ret = -1;

	do {
		if (!sm)
			break;

		if (upnp_wps_ctrlpt_start(sm->upnp_ctrlpt_sm, net_if))
			break;

		ret = 0;
	} while (0);

	return ret;

}


int wps_opt_upnp_sm_stop(struct wps_opt_upnp_sm *sm)
{
	int ret = -1;
	struct wps_config *conf;

	do {
		if (!sm)
			break;

		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   (sm->ctx->get_conf(sm->ctx->ctx))->wps:0;
		if (conf && conf->cur_upnp_device) {
			(void)wps_opt_upnp_set_selected_registrar(sm,
									conf->cur_upnp_device,
									0);
		}

		if (upnp_wps_ctrlpt_stop(sm->upnp_ctrlpt_sm))
			break;
		ret = 0;
	} while (0);

	return ret;

}


