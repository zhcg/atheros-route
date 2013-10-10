/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: upnp_wps_ctrlpt.c
//  Description: EAP-WPS UPnP control-point source
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

#include <upnp/ithread.h>
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ixml.h>

#include "common.h"
#include "upnp_wps_common.h"
#include "upnp_wps_ctrlpt.h"
#include "base64.h"
#include <stdlib.h>

#include <sys/ioctl.h>
#include <linux/if.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/route.h>
#include <errno.h>
#include <stdio.h>

#define WPS_MAXVARS		4
#define WPS_MAX_VAL_LEN	255
#define DEFAULT_TIMEOUT	1801

#define TIMER_LOOP_INC	30

struct wps_service {
	char *service_id;
	char *service_type;
	char *scpd_url;
	char *control_url;
	char *event_url;
	char *var[WPS_MAXVARS];
	char sid[sizeof(Upnp_SID) + 1];
};

struct wps_device{
	char *friendly_name;
	char *manufacturer;
	char *manufacturer_url;
	char *model_desc;
	char *model_name;
	char *model_number;
	char *model_url;
	char *serial_number;
	char *udn;
	char *upc;
	char *pres_url;
	int advr_timeout;
	struct wps_service service;
};

struct wps_device_node {
	struct wps_device device;
	struct wps_device_node *next;
};

struct upnp_wps_ctrlpt_sm {
	struct upnp_wps_ctrlpt_ctx *ctx;
	void *priv;
	int initialized;
	struct wps_device_node *device_list;
	ithread_mutex_t mutex_devlist;
	int mutex_initialized;
	UpnpClient_Handle ctrlpt_handle;
	ithread_t timer_thread;
	int quit_timer_thread;
};


static const char *wps_device_type = "urn:schemas-wifialliance-org:device:WFADevice:1";
static const char *wps_service_type = "urn:schemas-wifialliance-org:service:WFAWLANConfig:1";
enum WPS_EVENT {
	WPS_EVENT_WLANEVENT = 0,
	WPS_EVENT_APSTATUS,
	WPS_EVENT_STASTATUS
};
static const char *wps_event_name[] = {
	"WLANEvent", "APStatus", "STAStatus", 0
};

struct upnp_wps_action_callback_cookie {
	struct upnp_wps_ctrlpt_sm *sm;
	int (*callback)(struct upnp_wps_ctrlpt_sm *sm,
					char *control_url,
					IXML_Document *doc);
};

struct upnp_wps_get_var_callback_cookie {
	struct upnp_wps_ctrlpt_sm *sm;
	int device_no;
	int (*callback)(struct upnp_wps_ctrlpt_sm *sm,
					char *control_url,
					char *var, DOMString val);
};


struct upnp_wps_ctrlpt_sm *
upnp_wps_ctrlpt_init(struct upnp_wps_ctrlpt_ctx *ctx, void *priv)
{
	struct upnp_wps_ctrlpt_sm *sm = 0;
	do {
		sm = wpa_zalloc(sizeof(*sm));
		if (!sm)
			break;
		sm->ctx = ctx;
		sm->priv = priv;
		sm->ctrlpt_handle = -1;
		sm->timer_thread = -1;
	} while (0);

	return sm;
}


void
upnp_wps_ctrlpt_deinit(struct upnp_wps_ctrlpt_sm *sm)
{
	do {
		if (!sm)
			break;
		upnp_wps_ctrlpt_stop(sm);

		free(sm->ctx);
		free(sm);
	} while (0);
}

static int
upnp_wps_ctrlpt_delete_node(struct upnp_wps_ctrlpt_sm *sm,
							struct wps_device_node *node)
{
	int ret = -1;
	int res;
	int vars;

	do {
		if (!sm || !node)
			break;

		res = UpnpUnSubscribe(sm->ctrlpt_handle, node->device.service.sid);

		if (node->device.friendly_name)
			free(node->device.friendly_name);
		if (node->device.manufacturer)
			free(node->device.manufacturer);
		if (node->device.manufacturer_url)
			free(node->device.manufacturer_url);
		if (node->device.model_desc)
			free(node->device.model_desc);
		if (node->device.model_name)
			free(node->device.model_name);
		if (node->device.model_number)
			free(node->device.model_number);
		if (node->device.model_url)
			free(node->device.model_url);
		if (node->device.serial_number)
			free(node->device.serial_number);
		if (node->device.udn)
			free(node->device.udn);
		if (node->device.upc)
			free(node->device.upc);
		if (node->device.pres_url)
			free(node->device.pres_url);

		if (node->device.service.service_id)
			free(node->device.service.service_id);
		if (node->device.service.service_type)
			free(node->device.service.service_type);
		if (node->device.service.scpd_url)
			free(node->device.service.scpd_url);
		if (node->device.service.control_url)
			free(node->device.service.control_url);
		if (node->device.service.event_url)
			free(node->device.service.event_url);

		for (vars = 0; vars < WPS_MAXVARS; vars++) {
			if (node->device.service.var[vars])
				free(node->device.service.var[vars]);
		}

		free(node);
		node = 0;

		ret = 0;
	} while (0);

	return ret;
}


static int
upnp_wps_ctrlpt_remove_device(struct upnp_wps_ctrlpt_sm *sm,
							  char *udn)
{
	int ret = -1;
	struct wps_device_node *cur, *prev = 0;

	do {
		if (!udn)
			break;

		ithread_mutex_lock(&sm->mutex_devlist);

		cur = sm->device_list;
		while (cur) {
			if (!os_strcmp(cur->device.udn, udn)) {
				if (cur == sm->device_list)
					sm->device_list = cur->next;
				else if (prev)
					prev->next = cur->next;
				else
					break;
				upnp_wps_ctrlpt_delete_node(sm, cur);
				break;
			}
			prev = cur;
			cur = cur->next;
		}

		ithread_mutex_unlock(&sm->mutex_devlist);

		ret = 0;
	} while (0);

	return ret;
}


static int
upnp_wps_ctrlpt_remove_all_device(struct upnp_wps_ctrlpt_sm *sm)
{
	struct wps_device_node *cur, *next;

	ithread_mutex_lock(&sm->mutex_devlist);

	cur = sm->device_list;
	while (cur) {
		next = cur->next;
		upnp_wps_ctrlpt_delete_node(sm, cur);
		cur = next;
	}
	sm->device_list = 0;

	ithread_mutex_unlock(&sm->mutex_devlist);
	return 0;
}


int add_ssdp_network(char *net_if)
{
#define SSDP_TARGET		"239.0.0.0"
#define SSDP_NETMASK	"255.0.0.0"
	int ret = -1;
	SOCKET sock = -1;
	struct rtentry rt;
	struct sockaddr_in *sin;

	do {
		if (!net_if)
			break;

		memset(&rt, 0, sizeof(rt));
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (-1 == sock)
			break;

		rt.rt_dev = net_if;
		sin = (struct sockaddr_in *)&rt.rt_dst;
		sin->sin_family = AF_INET;
		sin->sin_port = 0;
		sin->sin_addr.s_addr = inet_addr(SSDP_TARGET);
		sin = (struct sockaddr_in *)&rt.rt_genmask;
		sin->sin_family = AF_INET;
		sin->sin_port = 0;
		sin->sin_addr.s_addr = inet_addr(SSDP_NETMASK);
		rt.rt_flags = RTF_UP;
		if (ioctl(sock, SIOCADDRT, &rt) < 0) {
			if (errno != EEXIST) {
                                perror("add_ssdp_network ioctl failed");
				break;
                        }
		}

		ret = 0;
	} while (0);

	if (-1 != sock)
		close(sock);

	return ret;
#undef SSDP_TARGET
#undef SSDP_NETMASK
}


int get_ip_address(char *net_if, char **ipaddr)
{
#define MAX_INTERFACES 256
	int ret = -1;
	char buf[MAX_INTERFACES * sizeof(struct ifreq)];
	struct ifconf conf;
	struct ifreq *req;
	struct sockaddr_in sock_addr;
	int sock = -1;
	int i;

	do {
		if (!ipaddr)
			break;
		*ipaddr = 0;

		if (!net_if)
			break;

		if(0 > (sock = socket(AF_INET, SOCK_DGRAM, 0)))
			break;

		conf.ifc_len = sizeof(buf);
		conf.ifc_ifcu.ifcu_buf = (caddr_t)buf;
		if (0 > ioctl(sock, SIOCGIFCONF, &conf))
			break;

		for( i = 0; i < conf.ifc_len; ) {
			req = (struct ifreq *)((caddr_t)conf.ifc_req + i);
			i += sizeof(*req);

			if (AF_INET == req->ifr_addr.sa_family) {
				if (!os_strcmp(net_if, req->ifr_name)) {
					size_t len;
					os_memcpy(&sock_addr, &req->ifr_addr, sizeof(req->ifr_addr));
					len = os_strlen(inet_ntoa(sock_addr.sin_addr)) + 1;
					*ipaddr = wpa_zalloc(len);
					if (!*ipaddr)
						break;
					os_snprintf(*ipaddr, len, "%s", inet_ntoa(sock_addr.sin_addr));
					ret = 0;
					break;
				}
			}
		}
	} while (0);

	if (0 <= sock)
		close(sock);

	return ret;
#undef MAX_INTERFACES
}


int get_mac_from_ip(char *ipaddr, char mac[18])
{
#define MAX_INTERFACES 256
	int ret = -1;
	char buf[MAX_INTERFACES * sizeof(struct ifreq)];
	struct ifconf conf;
	struct ifreq *req;
	struct sockaddr_in sock_addr;
	int sock = -1;
	int i;

	do {
		if(0 > (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)))
			break;

		conf.ifc_len = sizeof(buf);
		conf.ifc_ifcu.ifcu_buf = (caddr_t)buf;
		if (0 > ioctl(sock, SIOCGIFCONF, &conf))
			break;

		for( i = 0; i < conf.ifc_len; ) {
			req = (struct ifreq *)((caddr_t)conf.ifc_req + i);
			i += sizeof(*req);

			if (AF_INET == req->ifr_addr.sa_family) {
				os_memcpy(&sock_addr, &req->ifr_addr, sizeof(req->ifr_addr));
				if (!os_strcmp(ipaddr, inet_ntoa(sock_addr.sin_addr))) {
					os_snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
					(u8)(req->ifr_hwaddr.sa_data[0]),
					(u8)(req->ifr_hwaddr.sa_data[1]),
					(u8)(req->ifr_hwaddr.sa_data[2]),
					(u8)(req->ifr_hwaddr.sa_data[3]),
					(u8)(req->ifr_hwaddr.sa_data[4]),
					(u8)(req->ifr_hwaddr.sa_data[5]));
					ret = 0;
					break;
				}
			}
		}
	} while (0);

	if (0 <= sock)
		close(sock);

	return ret;
}

int
upnp_wps_ctrlpt_get_scan_results(struct upnp_wps_ctrlpt_sm *sm,
								 struct upnp_wps_ctrlpt_device_list **list)
{
	int ret = -1;
	struct wps_device_node *node;
	struct upnp_wps_ctrlpt_device_list *cur = 0;

	do {
		if (!sm || !list)
			break;

		ithread_mutex_lock(&sm->mutex_devlist);

		*list = 0;
		node = sm->device_list;
		while (node) {
			if (!*list) {
				*list = (struct upnp_wps_ctrlpt_device_list *)
						wpa_zalloc(sizeof(**list));
				if (!*list)
					break;
				cur = *list;
			} else if (cur) {
				cur->next = (struct upnp_wps_ctrlpt_device_list *)
						wpa_zalloc(sizeof(*cur));
				if (!cur->next)
					break;
				cur = cur->next;
			} else
				break;

			os_snprintf(cur->device.manufacturer,
						sizeof(cur->device.manufacturer),
						"%s", node->device.manufacturer);
			os_snprintf(cur->device.model_name,
						sizeof(cur->device.model_name),
						"%s", node->device.model_name);
			os_snprintf(cur->device.model_number,
						sizeof(cur->device.model_number),
						"%s", node->device.model_number);
			os_snprintf(cur->device.serial_number,
						sizeof(cur->device.serial_number),
						"%s", node->device.serial_number);
			os_snprintf(cur->device.udn,
						sizeof(cur->device.udn),
						"%s", node->device.udn);
			os_snprintf(cur->device.control_url,
						sizeof(cur->device.control_url),
						"%s", node->device.service.control_url);
			node = node->next;
		}

		ithread_mutex_unlock(&sm->mutex_devlist);

		if (node)
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


void
upnp_wps_ctrlpt_destroy_device_list(struct upnp_wps_ctrlpt_device_list *list)
{
	struct upnp_wps_ctrlpt_device_list *device, *next;
	do {
		if (!list)
			break;

		device = list;
		while (device) {
			next = device->next;
			free(device);
			device = next;
		}
	} while (0);
}


int
upnp_wps_ctrlpt_refresh_device(struct upnp_wps_ctrlpt_sm *sm, int timeout)
{
	int ret = -1;

	upnp_wps_ctrlpt_remove_all_device(sm);

	do {
		if (!timeout)
			timeout = TIMER_LOOP_INC; 

		if (UPNP_E_SUCCESS != UpnpSearchAsync(sm->ctrlpt_handle,
											  timeout,
											  wps_device_type, sm))
			break;
		ret = 0;
	} while (0);

	return ret;
}


static int
upnp_wps_ctrlpt_encode_base64(u8 *data, size_t data_len,
							  char **encoded, size_t *encoded_len)
{
	int ret = -1;

	do {
		if (!data || !encoded || !encoded_len)
			break;
		*encoded = 0;
		*encoded_len = 0;

		*encoded = base64_encode( (const unsigned char *)data,
								data_len, encoded_len, 72);
		if (!*encoded)
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (encoded && *encoded) {
			free(*encoded);
			*encoded = 0;
		}
		if (encoded_len)
			*encoded_len = 0;
	}

	return ret;
}


static int
upnp_wps_ctrlpt_decode_base64(char *data, size_t data_len,
								  u8 **decoded, size_t *decoded_len)
{
	int ret = -1;

	do {
		if (!data || !decoded || !decoded_len)
			break;
		*decoded = 0;
		*decoded_len = 0;

		*decoded = base64_decode((const unsigned char *)data,
								 data_len, decoded_len);
		if (!*decoded)
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (decoded && *decoded) {
			free(*decoded);
			*decoded = 0;
		}
		if (decoded_len)
			*decoded_len = 0;
	}

	return ret;
}


static int
upnp_wps_ctrlpt_get_device(struct upnp_wps_ctrlpt_sm *sm,
						   char *control_url,
						   struct wps_device_node **node)
{
	int ret = -1;
	struct wps_device_node *next;

	do {
		if (!sm || !control_url || !node)
			break;

		*node = 0;
		next = sm->device_list;
		while (next) {
			if (!os_strcmp(next->device.service.control_url, control_url)) {
				*node = next;
				break;
			}
			next = next->next;
		}

		if (!*node)
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (node && *node)
			*node = 0;
	}

	return ret;
}


static int
upnp_wps_ctrlpt_add_device(struct upnp_wps_ctrlpt_sm *sm,
						   IXML_Document *desc_doc,
						   char *location, int expires )
{
	int ret = -1;
	struct wps_device_node *node = 0, *next;
	char pres_url[250] = {0};
	char *url_base = 0, *device_type = 0, *friendly_name = 0,
		 *manufacturer = 0, *manufacturer_url = 0,
		 *model_desc = 0, *model_name = 0, *model_number = 0,
		 *model_url = 0, *serial_number = 0, *udn = 0, *upc = 0,
		 *rel_url = 0;
	char *service_id = 0,
		 *scpd_url = 0, *control_url = 0, *event_url = 0;
	int timeout = DEFAULT_TIMEOUT;
	Upnp_SID event_sid;
	int vars;

	if (!sm)
		return ret;

	ithread_mutex_lock(&sm->mutex_devlist);

	do {
		upnp_get_first_document_item(desc_doc, "URLBase", &url_base);
		upnp_get_first_document_item(desc_doc, "deviceType", &device_type);
		upnp_get_first_document_item(desc_doc, "friendlyName", &friendly_name);
		upnp_get_first_document_item(desc_doc, "manufacturer", &manufacturer);
		upnp_get_first_document_item(desc_doc, "manufacturerURL", &manufacturer_url);
		upnp_get_first_document_item(desc_doc, "modelDescription", &model_desc);
		upnp_get_first_document_item(desc_doc, "modelName", &model_name);
		upnp_get_first_document_item(desc_doc, "modelNumber", &model_number);
		upnp_get_first_document_item(desc_doc, "modelURL", &model_url);
		upnp_get_first_document_item(desc_doc, "serialNumber", &serial_number);
		upnp_get_first_document_item(desc_doc, "UDN", &udn);
		upnp_get_first_document_item(desc_doc, "UPC", &upc);
		upnp_get_first_document_item(desc_doc, "presentationURL", &rel_url);

		if (rel_url) {
			if (UPNP_E_SUCCESS != UpnpResolveURL((url_base?url_base:location), rel_url, pres_url))
				break;
		}

		if (os_strcmp(device_type, wps_device_type))
			break;

		node = sm->device_list;
		while (node) {
			if (!os_strcmp(node->device.udn, udn))
				break;
			node = node->next;
		}

		if (node) {
			node->device.advr_timeout = expires;
		} else {
			if (!upnp_find_service(desc_doc, location, (char *)wps_service_type,
				&service_id, &scpd_url, &control_url, &event_url))
				break;

			if (UPNP_E_SUCCESS != UpnpSubscribe(sm->ctrlpt_handle, event_url,
												&timeout, event_sid))
				break;

			node = (struct wps_device_node *)wpa_zalloc(sizeof(*node));
			if (friendly_name) {
				node->device.friendly_name =
						(char *)wpa_zalloc(os_strlen(friendly_name) + 1);
				if (!node->device.friendly_name)
					break;
				strcpy(node->device.friendly_name, friendly_name);
			}
			if (manufacturer) {
				node->device.manufacturer =
						(char *)wpa_zalloc(os_strlen(manufacturer) + 1);
				if (!node->device.manufacturer)
					break;
				strcpy(node->device.manufacturer, manufacturer);
			}
			if (manufacturer_url) {
				node->device.manufacturer_url =
					(char *)wpa_zalloc(os_strlen(manufacturer_url) + 1);
				if (!node->device.manufacturer_url)
					break;
				strcpy(node->device.manufacturer_url, manufacturer_url);
			}
			if (model_desc) {
				node->device.model_desc =
						(char *)wpa_zalloc(os_strlen(model_desc) + 1);
				if (!node->device.model_desc)
					break;
				strcpy(node->device.model_desc, model_desc);
			}
			if (model_name) {
				node->device.model_name =
						(char *)wpa_zalloc(os_strlen(model_name) + 1);
				if (!node->device.model_name)
					break;
				strcpy(node->device.model_name, model_name);
			}
			if (model_number) {
				node->device.model_number =
						(char *)wpa_zalloc(os_strlen(model_number) + 1);
				if (!node->device.model_number)
					break;
				strcpy(node->device.model_number, model_number);
			}
			if (model_url) {
				node->device.model_url =
						(char *)wpa_zalloc(os_strlen(model_url) + 1);
				if (!node->device.model_url)
					break;
				strcpy(node->device.model_url, model_url);
			}
			if (serial_number) {
				node->device.serial_number =
						(char *)wpa_zalloc(os_strlen(serial_number) + 1);
				if (!node->device.serial_number)
					break;
				strcpy(node->device.serial_number, serial_number);
			}
			if (udn) {
				node->device.udn =
						(char *)wpa_zalloc(os_strlen(udn) + 1);
				if (!node->device.udn)
					break;
				strcpy(node->device.udn, udn);
			}
			if (upc) {
				node->device.upc =
						(char *)wpa_zalloc(os_strlen(upc) + 1);
				if (!node->device.upc)
					break;
				strcpy(node->device.upc, upc);
			}
			if (rel_url) {
				node->device.pres_url =
						(char *)wpa_zalloc(os_strlen(pres_url) + 1);
				if (!node->device.pres_url)
					break;
				strcpy(node->device.pres_url, pres_url);
			}
			node->device.advr_timeout = expires;

			node->device.service.service_id =
					(char *)wpa_zalloc(os_strlen(service_id) + 1);
			if (!node->device.service.service_id)
				break;
			strcpy(node->device.service.service_id, service_id);
			node->device.service.service_type =
					(char *)wpa_zalloc(os_strlen(wps_service_type) + 1);
			if (!node->device.service.service_type)
				break;
			strcpy(node->device.service.service_type, wps_service_type);
			if (scpd_url) {
				node->device.service.scpd_url =
						(char *)wpa_zalloc(os_strlen(scpd_url) + 1);
				if (!node->device.service.scpd_url)
					break;
				strcpy(node->device.service.scpd_url, scpd_url);
			}
			if (control_url) {
				node->device.service.control_url =
						(char *)wpa_zalloc(os_strlen(control_url) + 1);
				if (!node->device.service.control_url)
					break;
				strcpy(node->device.service.control_url, control_url);
			}
			if (event_url) {
				node->device.service.event_url =
						(char *)wpa_zalloc(os_strlen(event_url) + 1);
				if (!node->device.service.event_url)
					break;
				strcpy(node->device.service.event_url, event_url);
			}
			os_memcpy(node->device.service.sid, event_sid, sizeof(Upnp_SID));

			for (vars = 0; vars < WPS_MAXVARS; vars++)
				node->device.service.var[vars] = (char *)wpa_zalloc(WPS_MAX_VAL_LEN);

			if (sm->device_list) {
				next = sm->device_list;
				while(next->next)
					next = next->next;
				next->next = node;
			} else
				sm->device_list = node;
		}
		ret = 0;
    } while (0);

	if (url_base) free(url_base);
	if (device_type) free(device_type);
	if (friendly_name) free(friendly_name);
	if (manufacturer) free(manufacturer);
	if (manufacturer_url) free(manufacturer_url);
	if (model_desc) free(model_desc);
	if (model_name) free(model_name);
	if (model_number) free(model_number);
	if (model_url) free(model_url);
	if (serial_number) free(serial_number);
	if (udn) free(udn);
	if (upc) free(upc);
	if (rel_url) free(rel_url);
	if (service_id) free(service_id);
	if (scpd_url) free(scpd_url);
	if (control_url) free(control_url);
	if (event_url) free(event_url);
	if (ret) {
		if (node)
			upnp_wps_ctrlpt_delete_node(sm, node);
	}
	ithread_mutex_unlock(&sm->mutex_devlist);

	return ret;
}


static void
upnp_wps_ctrlpt_handle_action_complete(char *control_url,
									   IXML_Document *doc,
									   struct upnp_wps_action_callback_cookie *cb_cookie)
{
	struct upnp_wps_ctrlpt_sm *sm;
	struct wps_device_node *node;
	do {
		if (!control_url || !doc || !cb_cookie)
			break;
		sm = cb_cookie->sm;
		if (!sm)
			break;

		ithread_mutex_lock(&sm->mutex_devlist);

		node = sm->device_list;
		while (node) {
			if (!os_strcmp(node->device.service.control_url, control_url)) {
				if (cb_cookie->callback)
					cb_cookie->callback(sm, control_url, doc);
				break;
			}
			node = node->next;
		}

		ithread_mutex_unlock(&sm->mutex_devlist);
	} while (0);

	if (cb_cookie)
		free(cb_cookie);
}


static void
upnp_wps_ctrlpt_handle_get_var_complete(char *control_url,
										char *var,
										DOMString val,
										struct upnp_wps_get_var_callback_cookie *cb_cookie)
{
	struct upnp_wps_ctrlpt_sm *sm;
	struct wps_device_node *node;
	do {
		if (!control_url || !var || !cb_cookie)
			break;
		sm = cb_cookie->sm;
		if (!sm)
			break;

		ithread_mutex_lock(&sm->mutex_devlist);

		node = sm->device_list;
		while (node) {
			if (!os_strcmp(node->device.service.control_url, control_url)) {
				cb_cookie->callback(sm, control_url, var, val);
				break;
			}
			node = node->next;
		}

		ithread_mutex_unlock(&sm->mutex_devlist);
	} while (0);

	if (cb_cookie)
		free(cb_cookie);
}


static void
upnp_wps_ctrlpt_handle_event_received(struct upnp_wps_ctrlpt_sm *sm,
									  Upnp_SID sid,
									  int event_key,
									  IXML_Document *doc)
{
	struct wps_device_node *node;
	IXML_NodeList *props, *vars;
	IXML_Element *prop, *var;
	int i, j, props_len, vars_len;
	char *val;

	do {
		if (!sm || !sm->ctx || !doc)
			break;

		ithread_mutex_lock(&sm->mutex_devlist);

		node = sm->device_list;
		while (node) {
			if (!os_strcmp(node->device.service.sid, sid)) {
				props = ixmlDocument_getElementsByTagName(doc, "e:property");
				if (!props)
					break;
				props_len = ixmlNodeList_length(props);
				for (i = 0; i < props_len; i++) {
					prop = (IXML_Element *)ixmlNodeList_item(props, i);
					for (j = 0; wps_event_name[j]; j++) {
						vars = ixmlElement_getElementsByTagName(prop,
												(char *)wps_event_name[j]);
						if (!vars)
							continue;

						vars_len = ixmlNodeList_length(vars);
						if (!vars_len) {
							ixmlNodeList_free(vars);
							continue;
						}

						var = (IXML_Element *)ixmlNodeList_item(vars, 0);
						do {
							val = 0;
							if (upnp_get_element_value(var, &val))
								break;
							switch (j) {
							case WPS_EVENT_WLANEVENT:
							{
								int event_type;
								char event_mac[18];
								u8 *decoded = 0;
								size_t decoded_len;
								do {
									if (!sm->ctx->received_wlan_event)
										break;

									if (upnp_wps_ctrlpt_decode_base64(val,
																	  os_strlen(val),
																	  &decoded,
																	  &decoded_len))
										break;

									event_type = (int)(u8)*decoded;
									strncpy(event_mac, (char *)(decoded + 1), sizeof(event_mac) - 1);
									event_mac[sizeof(event_mac) - 1] = 0;

									sm->ctx->received_wlan_event(sm->priv,
																 node->device.service.control_url,
																 event_type,
																 event_mac,
																 decoded + 18,
																 decoded_len - 18);
								} while (0);

								if (decoded)
									free(decoded);
								break;
							}
							case WPS_EVENT_APSTATUS:
								if (sm->ctx->received_ap_status)
									sm->ctx->received_ap_status(sm->priv,
																node->device.service.control_url,
																atoi(val));
								break;
							case WPS_EVENT_STASTATUS:
								if (sm->ctx->received_sta_status)
									sm->ctx->received_sta_status(sm->priv,
																 node->device.service.control_url,
																 atoi(val));
								break;
							}
						} while (0);

						if (val) {
							free(val);
							val = 0;
						}
						ixmlNodeList_free(vars);
					}
				}
				ixmlNodeList_free(props);
				break;
			}
			node = node->next;
		}

		ithread_mutex_unlock(&sm->mutex_devlist);
	} while (0);
}


static void
upnp_wps_ctrlpt_subscribe_update(struct upnp_wps_ctrlpt_sm *sm,
								 char *event_url,
								 Upnp_SID sid,
								 int timeout)
{
	struct wps_device_node *node;

	do {
		if (!sm || !event_url)
			break;

		ithread_mutex_lock(&sm->mutex_devlist);

		node = sm->device_list;
		while (node) {
			if (!os_strcmp(node->device.service.event_url, event_url)) {
				os_memset(node->device.service.sid, 0,
						  sizeof(node->device.service.sid));
				os_memcpy(node->device.service.sid, sid, sizeof(sid));
				break;
			}
			node = node->next;
		}

		ithread_mutex_unlock(&sm->mutex_devlist);
	} while (0);
}


static int
upnp_wps_ctrlpt_callback_event_handler(Upnp_EventType event_type,
									   void *event, void *cookie)
{
	switch (event_type) {
	/* SSDP */
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_SEARCH_RESULT:
	{
		struct upnp_wps_ctrlpt_sm *sm = (struct upnp_wps_ctrlpt_sm *)cookie;
		struct Upnp_Discovery *discovery_event =
				(struct Upnp_Discovery*)event;
		IXML_Document *desc_doc = 0;

		do {
			if (UPNP_E_SUCCESS != discovery_event->ErrCode)
				break;

			if (UPNP_E_SUCCESS !=
				UpnpDownloadXmlDoc(discovery_event->Location,
								   &desc_doc))
				break;

			if(upnp_wps_ctrlpt_add_device(sm, desc_doc,
										  discovery_event->Location,
										  discovery_event->Expires))
				break;
		} while (0);

		if (desc_doc)
			ixmlDocument_free(desc_doc);
		break;
	}
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		/* nothing to do */
		break;
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	{
		struct upnp_wps_ctrlpt_sm *sm = (struct upnp_wps_ctrlpt_sm *)cookie;
		struct Upnp_Discovery *discovery_event =
				(struct Upnp_Discovery*)event;

		if (UPNP_E_SUCCESS != discovery_event->ErrCode)
			;
		upnp_wps_ctrlpt_remove_device(sm, discovery_event->DeviceId);
		break;
	}
	/* SOAP */
	case UPNP_CONTROL_ACTION_COMPLETE:
	{
		struct upnp_wps_action_callback_cookie *cb_cookie
				= (struct upnp_wps_action_callback_cookie *)cookie;
		struct Upnp_Action_Complete *action_event =
			(struct Upnp_Action_Complete *)event;

		do {
			if (UPNP_E_SUCCESS != action_event->ErrCode)
				break;

			upnp_wps_ctrlpt_handle_action_complete(action_event->CtrlUrl,
												   action_event->ActionResult,
												   cb_cookie);
		} while (0);

		break;
	}
	case UPNP_CONTROL_GET_VAR_COMPLETE:
	{
		struct upnp_wps_get_var_callback_cookie *cb_cookie
				= (struct upnp_wps_get_var_callback_cookie *)cookie;
		struct Upnp_State_Var_Complete *var_event =
			( struct Upnp_State_Var_Complete * )event;
		do {
			if (UPNP_E_SUCCESS != var_event->ErrCode)
				break;

			upnp_wps_ctrlpt_handle_get_var_complete(var_event->CtrlUrl,
													var_event->StateVarName,
													var_event->CurrentVal,
													cb_cookie);
		} while (0);
		break;
	}
	/* GENA */
	case UPNP_EVENT_RECEIVED:
	{
		struct upnp_wps_ctrlpt_sm *sm = (struct upnp_wps_ctrlpt_sm *)cookie;
		struct Upnp_Event *e_event = (struct Upnp_Event *)event;
		upnp_wps_ctrlpt_handle_event_received(sm,
											  e_event->Sid, e_event->EventKey,
											  e_event->ChangedVariables);
		break;
	}
	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
	case UPNP_EVENT_RENEWAL_COMPLETE:
	{
		struct upnp_wps_ctrlpt_sm *sm = (struct upnp_wps_ctrlpt_sm *)cookie;
		struct Upnp_Event_Subscribe *es_event =
			(struct Upnp_Event_Subscribe *)event;

		do {
			if (UPNP_E_SUCCESS != es_event->ErrCode)
				break;

			upnp_wps_ctrlpt_subscribe_update(sm,
											 es_event->PublisherUrl,
											 es_event->Sid,
											 es_event->TimeOut);
		} while (0);
		
		break;
	}
	case UPNP_EVENT_AUTORENEWAL_FAILED:
	case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
	{
		struct upnp_wps_ctrlpt_sm *sm = (struct upnp_wps_ctrlpt_sm *)cookie;
		struct Upnp_Event_Subscribe *es_event =
			(struct Upnp_Event_Subscribe *)event;
		int timeout = DEFAULT_TIMEOUT;
		Upnp_SID sid;

		do 	{
			if (!sm)
				break;

			if (UPNP_E_SUCCESS !=
				UpnpSubscribe(sm->ctrlpt_handle, es_event->PublisherUrl,
							  &timeout, sid))
				break;

			upnp_wps_ctrlpt_subscribe_update(sm,
											 es_event->PublisherUrl,
											 sid, timeout);
		} while (0);

		break;
	}
	default:
		/* ignore */
		break;
	}

	return 0;
}


#if 0
static int
upnp_wps_ctrlpt_get_var(struct upnp_wps_ctrlpt_sm *sm,
						char *control_url,
						char *var,
						void *cookie)
{
	int ret = -1;
	struct wps_device_node *node;

	if (!sm)
		return ret;

	ithread_mutex_lock(&sm->mutex_devlist);

	do {
		if (!var)
			break;

		if (upnp_wps_ctrlpt_get_device(sm, control_url, &node))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpGetServiceVarStatusAsync(sm->ctrlpt_handle,
										 node->device.service.control_url,
										 var,
										 upnp_wps_ctrlpt_callback_event_handler,
										 cookie))
			break;

		ret = 0;
	} while (0);

	ithread_mutex_unlock(&sm->mutex_devlist);

	return ret;
}
#endif


static int
upnp_wps_ctrlpt_send_action(struct upnp_wps_ctrlpt_sm *sm,
							char *control_url,
							char *action,
							char **param_name,
							char **param_val,
							int param_cnt,
							void *cookie)
{
	int ret = -1;
	struct wps_device_node *node;
	IXML_Document *action_node = 0;
	int cnt;

	if (!sm)
		return ret;

	ithread_mutex_lock(&sm->mutex_devlist);

	do {
		if (upnp_wps_ctrlpt_get_device(sm, control_url, &node))
			break;

		if (!param_cnt)
			action_node = UpnpMakeAction(action, wps_service_type, 0, 0);
		else {
			for (cnt = 0; cnt < param_cnt; cnt++) {
				if (UPNP_E_SUCCESS !=
					UpnpAddToAction(&action_node, action, wps_service_type,
					param_name[cnt], param_val[cnt]))
					break;
			}

			if (cnt < param_cnt)
				break;
		}

		if (!action_node) break;

		if (UPNP_E_SUCCESS !=
			UpnpSendActionAsync(sm->ctrlpt_handle,
								node->device.service.control_url,
								wps_service_type,
								0, action_node,
								upnp_wps_ctrlpt_callback_event_handler, cookie))
			break;

		ret = 0;
	} while (0);

	ithread_mutex_unlock(&sm->mutex_devlist);

	if (action_node)
		ixmlDocument_free(action_node);

	return ret;
}

static int
upnp_wps_ctrlpt_resp_get_device_info(struct upnp_wps_ctrlpt_sm *sm,
									 char *control_url,
									 IXML_Document *doc)
{
	int ret = -1;
	IXML_NodeList *resps, *vars;
	IXML_Element *resp, *var;
	int i, resps_len, vars_len;
	char *val;
	u8 *decoded;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !doc)
			break;
		if (!sm->ctx->received_resp_get_device_info)
			break;

		resps = ixmlDocument_getElementsByTagName(doc, "u:GetDeviceInfoResponse");
		if (!resps)
			break;
		resps_len = ixmlNodeList_length(resps);
		for (i = 0; i < resps_len; i++) {
			resp = (IXML_Element *)ixmlNodeList_item(resps, i);
			vars = ixmlElement_getElementsByTagName(resp, "NewDeviceInfo");
			if (!vars)
				continue;

			vars_len = ixmlNodeList_length(vars);
			if (!vars_len)
				continue;

			var = (IXML_Element *)ixmlNodeList_item(vars, 0);
			do {
				decoded = 0;
				val = 0;
				if (upnp_get_element_value(var, &val))
					break;

				if (upnp_wps_ctrlpt_decode_base64(val,
												  os_strlen(val),
												  &decoded,
												  &decoded_len))
					break;

				if(sm->ctx->received_resp_get_device_info(sm->priv,
											 control_url,
											 decoded,
											 decoded_len))
					break;
			} while (0);

			if (decoded)
				free(decoded);
			if (val)
				free(val);
			ixmlNodeList_free(vars);
		}
		ixmlNodeList_free(resps);

		ret = 0;
	} while (0);

	return ret;
}


int
upnp_wps_ctrlpt_send_get_device_info(struct upnp_wps_ctrlpt_sm *sm,
									 char *control_url)
{
	int ret = -1;
	struct upnp_wps_action_callback_cookie *a_cb;

	do {
		if (!sm || !control_url)
			break;

		a_cb = (struct upnp_wps_action_callback_cookie *)wpa_zalloc(sizeof(*a_cb));
		if (!a_cb)
			break;
		a_cb->sm = sm;
		a_cb->callback = upnp_wps_ctrlpt_resp_get_device_info;

		if (upnp_wps_ctrlpt_send_action(sm,
										control_url,
										"GetDeviceInfo",
										0, 0, 0, (void *)a_cb))
			break;

		ret = 0;
	} while (0);

	return ret;
}


static int
upnp_wps_ctrlpt_resp_put_message(struct upnp_wps_ctrlpt_sm *sm,
								 char *control_url,
								 IXML_Document *doc)
{
	int ret = -1;
	IXML_NodeList *resps, *vars;
	IXML_Element *resp, *var;
	int i, resps_len, vars_len;
	char *val;
	u8 *decoded;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !doc)
			break;
		if (!sm->ctx->received_resp_put_message)
			break;

		resps = ixmlDocument_getElementsByTagName(doc, "u:PutMessageResponse");
		if (!resps)
			break;
		resps_len = ixmlNodeList_length(resps);
		for (i = 0; i < resps_len; i++) {
			resp = (IXML_Element *)ixmlNodeList_item(resps, i);
			vars = ixmlElement_getElementsByTagName(resp, "NewOutMessage");
			if (!vars)
				continue;

			vars_len = ixmlNodeList_length(vars);
			if (!vars_len)
				continue;

			var = (IXML_Element *)ixmlNodeList_item(vars, 0);
			do {
				decoded = 0;
				val = 0;
				if (upnp_get_element_value(var, &val))
					break;

				if (upnp_wps_ctrlpt_decode_base64(val,
												  os_strlen(val),
												  &decoded,
												  &decoded_len))
					break;

				if(sm->ctx->received_resp_put_message(sm->priv,
											 control_url,
											 decoded,
											 decoded_len))
					break;
			} while (0);

			if (decoded)
				free(decoded);
			if (val)
				free(val);
			ixmlNodeList_free(vars);
		}
		ixmlNodeList_free(resps);

		ret = 0;
	} while (0);

	return 0;
}


int
upnp_wps_ctrlpt_send_put_message(struct upnp_wps_ctrlpt_sm *sm,
								 char *control_url,
								 u8 *msg, size_t msg_len)
{
	int ret = -1;
	char *param_name = "NewInMessage";
	char *encoded = 0;
	size_t encoded_len;
	struct upnp_wps_action_callback_cookie *a_cb;

	do {
		if (!sm || !control_url || !msg)
			break;

		a_cb = (struct upnp_wps_action_callback_cookie *)wpa_zalloc(sizeof(*a_cb));
		if (!a_cb)
			break;
		a_cb->sm = sm;
		a_cb->callback = upnp_wps_ctrlpt_resp_put_message;

		if (upnp_wps_ctrlpt_encode_base64(msg, msg_len, &encoded, &encoded_len))
			break;

		if (upnp_wps_ctrlpt_send_action(sm,
										control_url,
										"PutMessage",
										&param_name,
										&encoded, 1,
										(void *)a_cb))
			break;

		ret = 0;
	} while (0);

	if (encoded)
		free(encoded);

	return ret;
}


int
upnp_wps_ctrlpt_send_put_wlan_response(struct upnp_wps_ctrlpt_sm *sm,
									   char *control_url, int ev_type,
									   u8 *msg, size_t msg_len)
{
	int ret = -1;
	char *param_name[3] = {"NewMessage", "NewWLANEventType", "NewWLANEventMAC"};
	char *param_val[3];
	char *encoded = 0;
	size_t encoded_len;
	char type[2];
	char mac[32];
	struct upnp_wps_action_callback_cookie *a_cb;

	do {
		if (!sm || !control_url || !msg)
			break;

		a_cb = (struct upnp_wps_action_callback_cookie *)wpa_zalloc(sizeof(*a_cb));
		if (!a_cb)
			break;
		a_cb->sm = sm;
		a_cb->callback = 0;

		if (upnp_wps_ctrlpt_encode_base64(msg, msg_len, &encoded, &encoded_len))
			break;
		os_snprintf(type, sizeof(type), "%d",
				 ev_type & (UPNP_WPS_WLANEVENT_TYPE_PROBE|
				 			UPNP_WPS_WLANEVENT_TYPE_EAP));
		if (get_mac_from_ip(UpnpGetServerIpAddress(), mac))
			break;

		param_val[0] = encoded;
		param_val[1] = type;
		param_val[2] = mac;

		if (upnp_wps_ctrlpt_send_action(sm,
										control_url,
										"PutWLANResponse",
										param_name,
										param_val, 3, (void *)a_cb))
			break;

		ret = 0;
	} while (0);

	if (encoded)
		free(encoded);

	return ret;
}

int
upnp_wps_ctrlpt_send_set_selected_registrar(struct upnp_wps_ctrlpt_sm *sm,
											char *control_url,
											u8 *msg, size_t msg_len)
{
	int ret = -1;
	char *param_name = "NewMessage";
	char *encoded = 0;
	size_t encoded_len;
	struct upnp_wps_action_callback_cookie *a_cb;

	do {
		if (!sm || !control_url || !msg)
			break;

		a_cb = (struct upnp_wps_action_callback_cookie *)wpa_zalloc(sizeof(*a_cb));
		if (!a_cb)
			break;
		a_cb->sm = sm;
		a_cb->callback = 0;

		if (upnp_wps_ctrlpt_encode_base64(msg, msg_len, &encoded, &encoded_len))
			break;

		if (upnp_wps_ctrlpt_send_action(sm,
										control_url,
										"SetSelectedRegistrar",
										&param_name,
										&encoded, 1, (void *)a_cb))
			break;

		ret = 0;
	} while (0);

	if (encoded)
		free(encoded);

	return ret;
}

static void *
upnp_wps_ctrlpt_timer(void *args)
{
	struct upnp_wps_ctrlpt_sm *sm = (struct upnp_wps_ctrlpt_sm *)args;
	struct wps_device_node *prev, *cur;

	if (!sm)
		return 0;

	sm->quit_timer_thread = 0;
	while (!sm->quit_timer_thread) {
		isleep(TIMER_LOOP_INC);

		ithread_mutex_lock(&sm->mutex_devlist);
		prev = 0;
		cur = sm->device_list;
		while (cur) {
			cur->device.advr_timeout -= TIMER_LOOP_INC;
			if (0 >= cur->device.advr_timeout) {
				if (cur == sm->device_list)
					sm->device_list = cur->next;
				else
					prev->next = cur->next;
				upnp_wps_ctrlpt_delete_node(sm, cur);
				if (prev)
					cur = prev->next;
				else
					cur = sm->device_list;
			} else {
				if (cur->device.advr_timeout < (2 * TIMER_LOOP_INC)) {
					(void)UpnpSearchAsync(sm->ctrlpt_handle,
										  TIMER_LOOP_INC,
										  cur->device.udn, sm);
				}
				prev = cur;
				cur = cur->next;
			}
		}
		ithread_mutex_unlock(&sm->mutex_devlist);
	}

	return 0;
}


int
upnp_wps_ctrlpt_start(struct upnp_wps_ctrlpt_sm *sm, char *net_if)
{
	int ret = -1;
	char *ip_address = 0;

	do {
		if (!sm)
			break;

		if (sm->initialized)
			upnp_wps_ctrlpt_stop(sm);

		ithread_mutex_init(&sm->mutex_devlist, 0);
		sm->mutex_initialized = 1;

		if (add_ssdp_network(net_if))
			break;

		if (get_ip_address(net_if, &ip_address))
			break;

		if (UPNP_E_SUCCESS != UpnpInit(ip_address, 0))
			break;
		sm->initialized++;

		if (os_strcmp(UpnpGetServerIpAddress(), ip_address))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpRegisterClient(upnp_wps_ctrlpt_callback_event_handler,
							   (void *)sm, &sm->ctrlpt_handle))
			break;
		sm->initialized++;

		if (ithread_create(&sm->timer_thread, 0, upnp_wps_ctrlpt_timer, (void *)sm)) {
			sm->timer_thread = -1;
			break;
		}
		sm->initialized++;

		(void)upnp_wps_ctrlpt_refresh_device(sm, 5);

		ret = 0;
	} while (0);

	if (ip_address)
		free(ip_address);

	return ret;
}


int
upnp_wps_ctrlpt_stop(struct upnp_wps_ctrlpt_sm *sm)
{
	do {
		if (!sm)
			break;

		if (!sm->initialized)
			break;

		if (0 <= (int)sm->timer_thread) {
			sm->quit_timer_thread = 1;
			ithread_join(sm->timer_thread, 0);
			sm->timer_thread = -1;
			sm->initialized--;
		}

		if (sm->mutex_initialized)
			ithread_mutex_lock(&sm->mutex_devlist);
		if (0 <= (int)sm->ctrlpt_handle) {
			UpnpUnRegisterClient(sm->ctrlpt_handle);
			sm->ctrlpt_handle = -1;
			sm->initialized--;
		}

		if (sm->initialized) {
			UpnpFinish();
			sm->initialized--;
		}
		if (sm->mutex_initialized) {
			ithread_mutex_unlock(&sm->mutex_devlist);
			ithread_mutex_destroy(&sm->mutex_devlist);
			sm->mutex_initialized = 0;
		}
	} while (0);

	if (sm)
		sm->initialized = 0;
	return 0;
}


