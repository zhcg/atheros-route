/*
 * WPA Supplicant / Wi-Fi Simple Configuration 7C Proposal
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2005 Intel Corporation. All rights reserved.
 * Contact Information: Harsha Hegde  <harsha.hegde@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README, README_WPS and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/*#include <netinet/in.h> */
#ifdef __linux__
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/rand.h>

#include "common.h"
#include "eloop.h"
#include "eap_i.h"
#include "wpa_supplicant.h"
#include "intel_eap_wps.h"
#include "intel_udplib.h"

#if !defined(DIFF_PORT_FROM_HOSTAPD)
#define WPS_EAP_UDP_PORT            37000
#else   // !defined(DIFF_PORT_FROM_HOSTAPD)
#define WPS_EAP_UDP_PORT            37002
#endif  // !defined(DIFF_PORT_FROM_HOSTAPD)
#define WPS_EAP_UDP_ADDR            "127.0.0.1"
// #define WPS_COMMANDS_UDP_PORT       38000

/*
static void wps_read_callback(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct eap_wps_data *data = eloop_ctx;
	struct wps_command_data comData;
	int recvBytes;
	struct sockaddr_in from;

	wpa_printf(MSG_DEBUG, "EAP-WPS: Entered wps_read_callback. "
			"sock = %d\n", sock);

	recvBytes = udp_read(data->udpFdCom, (u8 *) &comData, 
			sizeof(struct wps_command_data), &from);

	if (recvBytes == -1 || recvBytes < sizeof(struct wps_command_data))
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Reading Command message "
				"from upper layer failed\n");
		return;
	}

	if (comData.type == WPS_CTYPE_NEW_SETTINGS)
	{
		// write file; send command to reload
	}
	else
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Wrong command type from upper layer\n");
		return;
	}
   
	return;
}
*/

static void * eap_wps_init(struct eap_sm *sm)
{
	struct eap_wps_data *data;

	wpa_printf(MSG_DEBUG,"@#*@#*@#*EAP-WPS: Entered eap_wps_init *#@*#@*#@");

	data = os_malloc(sizeof(*data));
	if (data == NULL)
		return data;
	os_memset(data, 0, sizeof(*data));

	data->udpFdEap = udp_open();

	/*
	data->udpFdCom = udp_open();
	if (udp_bind(data->udpFdCom, WPS_COMMANDS_UDP_PORT) == -1)
	{
		wpa_printf(MSG_DEBUG, "EAP-WPS: udp_bind failed!");
		os_free(data);
		return NULL;
	}

	eloop_register_read_sock(data->udpFdCom, wps_read_callback,
				 data, NULL);
	*/

	sm->eap_method_priv = data;

	return data;
}


static void eap_wps_deinit(struct eap_sm *sm, void *priv)
{
	wpa_printf(MSG_DEBUG,"@#*@#*@#*EAP-WPS: Entered eap_wps_reset *#@*#@*#@");

	struct eap_wps_data *data = (struct eap_wps_data *)priv;
	if (data == NULL)
		return;

	if (data->udpFdEap != -1)
	{
		udp_close(data->udpFdEap);
		data->udpFdEap = -1;
	}

	/*
	if (data->udpFdCom != -1)
	{
		eloop_unregister_read_sock(data->udpFdCom);
		udp_close(data->udpFdCom);
		data->udpFdCom = -1;
	}
	*/

	os_free(data);
}

static u8 * eap_wps_process(struct eap_sm *sm, void *priv,
							struct eap_method_ret *ret,
							const u8 *reqData, size_t reqDataLen,
							size_t *respDataLen)
{
	struct eap_wps_data *data = priv;
	struct eap_hdr *req;
	int recvBytes;
	u8 * resp;
	u8 * sendBuf;
	u32 sendBufLen;
	struct sockaddr_in from;
	struct sockaddr_in to;
	WPS_NOTIFY_DATA notifyData;
	WPS_NOTIFY_DATA * recvNotify;

	wpa_printf(MSG_DEBUG,"@#*@#*@#*EAP-WPS: Entered eap_wps_process *#@*#@*#@");

	req = (struct eap_hdr *) reqData;
	wpa_printf(MSG_DEBUG, "EAP-WPS : Received packet(len=%lu) ",
			   (unsigned long) reqDataLen);
	if(ntohs(req->length) != reqDataLen)
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Pkt length in pkt(%d) differs from" 
			" supplied (%d)\n", ntohs(req->length), reqDataLen);
		ret->ignore = TRUE;
		return NULL;
	}

	notifyData.type = WPS_NOTIFY_TYPE_PROCESS_REQ;
	notifyData.length = reqDataLen;
	notifyData.u.process.state = data->state;
   
	sendBuf = (u8 *) os_malloc(sizeof(WPS_NOTIFY_DATA) + reqDataLen);
	if ( ! sendBuf)
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Memory allocation "
				"for the sendBuf failed\n");
		ret->ignore = TRUE;
		return NULL;
	}

	os_memcpy(sendBuf, &notifyData, sizeof(WPS_NOTIFY_DATA));
	os_memcpy(sendBuf + sizeof(WPS_NOTIFY_DATA), reqData, reqDataLen);
	sendBufLen = sizeof(WPS_NOTIFY_DATA) + reqDataLen;

	to.sin_addr.s_addr = inet_addr(WPS_EAP_UDP_ADDR);
	to.sin_family = AF_INET;
	to.sin_port = host_to_be16(WPS_EAP_UDP_PORT);

	if (udp_write(data->udpFdEap, (char *) sendBuf, sendBufLen, &to) < 
			sendBufLen)
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Sending Eap message to "
				"upper Layer failed\n");
		ret->ignore = TRUE;
		os_free(sendBuf);
		return NULL;
	}

	os_free(sendBuf);

	recvBytes = udp_read_timed(data->udpFdEap, (char *) data->recvBuf, 
			WPS_RECVBUF_SIZE, &from, 5);

	if (recvBytes == -1)
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Reading EAP message "
				"from upper layer failed\n");
		ret->ignore = TRUE;
		return NULL;
	}

	recvNotify = (WPS_NOTIFY_DATA *) data->recvBuf;
	if ( (recvNotify->type != WPS_NOTIFY_TYPE_PROCESS_RESULT) ||
	//     (recvNotify->length == 0) ||
		 (recvNotify->u.processResult.result != WPS_NOTIFY_RESULT_SUCCESS) )
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Process Message failed "
				"somewhere\n");
		ret->ignore = TRUE;
		return NULL;
	}
	
	resp = (u8 *) os_malloc(recvNotify->length);
	if ( ! resp)
	{
		wpa_printf(MSG_INFO, "EAP-WPS: Memory allocation "
				"for the resp failed\n");
		ret->ignore = TRUE;
		return NULL;
	}

	os_memcpy(resp, recvNotify + 1, recvNotify->length);
	*respDataLen = recvNotify->length;
	ret->ignore = FALSE;
	ret->decision = DECISION_COND_SUCC;
	ret->allowNotifications = FALSE;

	/*check if we're done*/
	if (recvNotify->u.processResult.done)
	{
		ret->methodState = METHOD_DONE;
	}
	else
	{
		wpa_printf(MSG_INFO, "Always setting it to METHOD_CONT\n");
		ret->methodState = METHOD_CONT;
	}

	return resp;
}

int eap_peer_wps_register(void)
{
#define EAP_VENDOR_ID_WPS	0x0000372a
#define EAP_VENDOR_TYPE_WPS	1

	struct eap_method *eap;
	int ret;

	eap = eap_peer_method_alloc(EAP_PEER_METHOD_INTERFACE_VERSION,
					EAP_VENDOR_ID_WPS, EAP_VENDOR_TYPE_WPS, "WPS");
	if (eap == NULL)
		return -1;

	eap->init = eap_wps_init;
	eap->deinit = eap_wps_deinit;
	eap->process = eap_wps_process;

	ret = eap_peer_method_register(eap);
	if (ret)
		eap_peer_method_free(eap);
	return ret;
}

