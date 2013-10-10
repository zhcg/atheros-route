/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: upnp_wps_ctrlpt_win.c
//  Description: EAP-WPS UPnP control-point source for Microsoft Windows
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

#ifndef COBJMACROS
#define COBJMACROS
#endif /* COBJMACROS */

#include "includes.h"

#include "common.h"
#include "upnp_wps_ctrlpt.h"

#include <upnp.h>

static const wchar_t *wps_device_type = L"urn:schemas-wifialliance-org:device:WFADevice:1";
static const wchar_t *wps_service_type = L"urn:schemas-wifialliance-org:service:WFAWLANConfig:1";
static const wchar_t *wps_service_id = L"urn:wifialliance-org:serviceId:WFAWLANConfig1";

struct wps_device;

struct upnp_service_callback {
	IUPnPServiceCallbackVtbl *lpVtbl;
	struct upnp_wps_ctrlpt_sm *sm;
	struct wps_device *device;
	u32 ref_count;
};

struct wps_device {
	IUPnPDevice *handle;
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
	char *control_url;
	IUPnPServices *services;
	IUPnPService *service;
	struct upnp_service_callback *service_callback;
};

struct wps_device_node {
	struct wps_device device;
	struct wps_device_node *next;
};

struct upnp_device_finder_callback {
	struct IUPnPDeviceFinderCallbackVtbl *lpVtbl;
	struct upnp_wps_ctrlpt_sm *sm;
	u32 ref_count;
};

struct upnp_wps_ctrlpt_sm {
	struct upnp_wps_ctrlpt_ctx *ctx;
	void *priv;
	int co_initialized;
	int started;
	IUPnPDeviceFinder *device_finder;
	u32 async_finder;
	struct upnp_device_finder_callback *device_finder_callback;
	struct wps_device_node *device_list;
	HANDLE mutex_devlist;
};

static int upnp_wps_ctrlpt_add_device(struct upnp_wps_ctrlpt_sm *sm, IUPnPDevice *device);
static int upnp_wps_ctrlpt_remove_device(struct upnp_wps_ctrlpt_sm *sm, char *udn);
static void upnp_wps_ctrlpt_handle_event_received(struct upnp_wps_ctrlpt_sm *sm, struct wps_device *device, char *var_name,
												  void *value, size_t value_length);

static int
convert_bstr_to_char(BSTR bstr, char **chr)
{
	int ret = -1;
	size_t len;

	do {
		if (!bstr || !chr)
			break;
		*chr = 0;

		len = WideCharToMultiByte(CP_ACP, 0, (OLECHAR*)bstr, -1, 0, 0, 0, 0);

		*chr = wpa_zalloc(len + 1);
		if (!*chr)
			break;

		if (!WideCharToMultiByte(CP_ACP, 0, (OLECHAR*)bstr, -1, *chr, len, 0, 0))
			break;

		ret = 0;
	} while (0);

	if (ret && !chr && !*chr) {
		os_free(*chr);
		*chr = 0;
	}

	return ret;
}


static int
convert_char_to_bstr(char *chr, BSTR *bstr)
{
	int ret = -1;
	size_t len;
	wchar_t *wstr = 0;

	do {
		if (!bstr || !chr)
			break;
		*bstr = 0;

		len = os_strlen(chr);

		wstr = wpa_zalloc((len + 1) * sizeof(*wstr));
		if (!wstr)
			break;

		_snwprintf(wstr, len + 1, L"%S", chr);
		*bstr = SysAllocString(wstr);
		if (!*bstr)
			break;

		ret = 0;
	} while (0);

	if (ret && !bstr && !*bstr) {
		SysFreeString(*bstr);
		*bstr = 0;
	}

	if (wstr)
		os_free(wstr);

	return ret;
}


static HRESULT STDMETHODCALLTYPE
service_callback_query_interface(IUPnPServiceCallback *This, REFIID iid, LPVOID* ppvObject)
{
	HRESULT hr = S_OK;
	
	do {
		if(!ppvObject) {
			hr = E_POINTER;
			break;
		}
		*ppvObject = NULL;
	
		if(!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IUPnPServiceCallback)) {
			hr = E_NOINTERFACE;
			break;
		}

		*ppvObject = This;
		IUPnPServiceCallback_AddRef(This);
	} while (0);
	
	return hr;
}

static ULONG STDMETHODCALLTYPE
service_callback_add_ref(IUPnPServiceCallback *This)
{
	struct upnp_service_callback *service_callback =
			(struct upnp_service_callback *)This;
	return InterlockedIncrement(&service_callback->ref_count);
}

static ULONG STDMETHODCALLTYPE
service_callback_release(IUPnPServiceCallback * This)
{
	struct upnp_service_callback *service_callback =
			(struct upnp_service_callback *)This;
	ULONG ref_count = 0;

	do {
		if (!service_callback->ref_count)
			break;

		(void)InterlockedDecrement(&service_callback->ref_count);
		ref_count = service_callback->ref_count;
		if (!service_callback->ref_count) {
			os_free(service_callback->lpVtbl);
			os_free(service_callback);
		}
	} while (0);
	return ref_count;
}

static HRESULT STDMETHODCALLTYPE
service_callback_state_variable_changed(IUPnPServiceCallback *This,
										IUPnPService *pus,
										LPCWSTR pcwszStateVarName,
										VARIANT vaValue)
{
	HRESULT hr = S_FALSE;
	struct upnp_service_callback *service_callback =
			(struct upnp_service_callback *)This;
	struct upnp_wps_ctrlpt_sm *sm = service_callback->sm;
	struct wps_device_node *node;
	struct wps_device *device;
	BSTR bstrSid = 0, bstrSid2 = 0, bstrStid = 0, bstrStid2 = 0;
	char *csid = 0, *csid2 = 0, *cstid = 0, *cstid2 = 0;
	char var_name[0x100];
	void *value = 0;
	size_t value_length = 0, ubound, lbound;
	char *cvalue = 0;

	WaitForSingleObject(sm->mutex_devlist, INFINITE);

	do {

		if (!pus)
			break;

		if (!SUCCEEDED(IUPnPService_get_Id(pus, &bstrSid)))
			break;

		if (convert_bstr_to_char(bstrSid, &csid))
			break;

		if (!SUCCEEDED(IUPnPService_get_ServiceTypeIdentifier(pus, &bstrStid)))
			break;

		if (convert_bstr_to_char(bstrStid, &cstid))
			break;

		os_snprintf(var_name, sizeof(var_name), "%S", pcwszStateVarName);

		node = sm->device_list;
		while (node) {
			do {
				device = &node->device;

				if (!SUCCEEDED(IUPnPService_get_Id(device->service, &bstrSid2)))
					break;

				if (convert_bstr_to_char(bstrSid2, &csid2))
					break;

				if (!SUCCEEDED(IUPnPService_get_ServiceTypeIdentifier(device->service, &bstrStid2)))
					break;

				if (convert_bstr_to_char(bstrStid2, &cstid2))
					break;

				if ((device->service_callback != service_callback) ||
					os_strcmp(csid, csid2) || os_strcmp(cstid, cstid2))
					break;

				switch (V_VT(&vaValue)) {
				case VT_ARRAY|VT_UI1:
					ubound = 0;
					lbound = 0;
					if (!SUCCEEDED(SafeArrayGetUBound(V_ARRAY(&vaValue), 1, (LONG *)&ubound)))
						break;
					if (!SUCCEEDED(SafeArrayGetLBound(V_ARRAY(&vaValue), 1, (LONG *)&lbound)))
						break;
					value_length = ubound - lbound + 1;
					if (!SUCCEEDED(SafeArrayAccessData(V_ARRAY(&vaValue), &value)))
						break;
					break;
				case VT_UI1:
					(u8*)value = &vaValue.bVal;
					value_length = 1;
					break;
				case VT_BSTR:
					if (convert_bstr_to_char(vaValue.bstrVal, &cvalue))
						break;
					value = cvalue;
					value_length = os_strlen(cvalue);
					break;
				default:
					break;
				}

				if (value && value_length) {
					upnp_wps_ctrlpt_handle_event_received(service_callback->sm, device, var_name, value, value_length);

					switch (V_VT(&vaValue)) {
					case VT_ARRAY|VT_UI1:
						(void)SafeArrayUnaccessData(V_ARRAY(&vaValue));
						break;
					case VT_UI1:
						break;
					case VT_BSTR:
						if (cvalue) os_free(cvalue); cvalue = 0;
						break;
					default:
						break;
					}
				}
			} while(0);

			node = node->next;

			if (bstrSid2) SysFreeString(bstrSid2); bstrSid2 = 0;
			if (csid2) os_free(csid2); csid2 = 0;
			if (bstrStid2) SysFreeString(bstrStid2); bstrStid2 = 0;
			if (cstid2) os_free(cstid2); csid2 = 0;
		}

		hr = S_OK;
	} while (0);

	ReleaseMutex(sm->mutex_devlist);

	if (bstrSid) SysFreeString(bstrSid); bstrSid = 0;
	if (csid) os_free(csid); csid = 0;
	if (bstrStid) SysFreeString(bstrStid); bstrStid = 0;
	if (cstid) os_free(cstid); cstid = 0;

	return hr;
}


static HRESULT STDMETHODCALLTYPE
service_callback_service_instance_died(IUPnPServiceCallback *This,
									   IUPnPService *pus)
{
	HRESULT hr = S_FALSE;
	struct upnp_service_callback *service_callback =
			(struct upnp_service_callback *)This;

	do {
		if (!pus)
			break;

		hr = S_OK;
	} while (0);

	return hr;
}


static int
destroy_service_callback(struct upnp_wps_ctrlpt_sm *sm,
						 struct wps_device *device,
						 struct upnp_service_callback *service_callback)
{
	int ret = -1;

	do {
		if (!service_callback)
			break;

		IUPnPServiceCallback_Release((IUPnPServiceCallback*)service_callback);

		if (device->service_callback == service_callback)
			device->service_callback = 0;

		ret = 0;
	} while (0);

	return ret;
}


static struct upnp_service_callback *
create_service_callback(struct upnp_wps_ctrlpt_sm *sm,
							  struct wps_device *device)
{
	int ret = -1;
	struct upnp_service_callback *service_callback = 0;

	do {
		if (!sm)
			break;

		service_callback = wpa_zalloc(sizeof(*service_callback));
		service_callback->sm = sm;
		service_callback->device = device;
		service_callback->lpVtbl= wpa_zalloc(sizeof(*service_callback->lpVtbl));
		if (!service_callback->lpVtbl)
			break;
		service_callback->lpVtbl->QueryInterface = service_callback_query_interface;
		service_callback->lpVtbl->AddRef = service_callback_add_ref;
		service_callback->lpVtbl->Release = service_callback_release;
		service_callback->lpVtbl->StateVariableChanged = service_callback_state_variable_changed;
		service_callback->lpVtbl->ServiceInstanceDied = service_callback_service_instance_died;

		IUPnPServiceCallback_AddRef((IUPnPServiceCallback *)service_callback);

		ret = 0;
	} while (0);

	if (ret) {
		destroy_service_callback(sm, device, service_callback);
		service_callback = 0;
	}

	return service_callback;
}


static HRESULT STDMETHODCALLTYPE
device_finder_callback_query_interface(IUPnPDeviceFinderCallback *This, REFIID iid, LPVOID* ppvObject)
{
	HRESULT hr = S_OK;
	
	do {
		if(!ppvObject) {
			hr = E_POINTER;
			break;
		}
		*ppvObject = NULL;
	
		if(!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IUPnPDeviceFinderCallback)) {
			hr = E_NOINTERFACE;
			break;
		}

		*ppvObject = This;
		IUPnPDeviceFinderCallback_AddRef(This);
	} while (0);
	
	return hr;
}

static ULONG STDMETHODCALLTYPE
device_finder_callback_add_ref(IUPnPDeviceFinderCallback *This)
{
	struct upnp_device_finder_callback *device_finder_callback =
			(struct upnp_device_finder_callback *)This;
	return InterlockedIncrement(&device_finder_callback->ref_count);
}

static ULONG STDMETHODCALLTYPE
device_finder_callback_release(IUPnPDeviceFinderCallback * This)
{
	struct upnp_device_finder_callback *device_finder_callback =
			(struct upnp_device_finder_callback *)This;
	ULONG ref_count = 0;

	do {
		if (!device_finder_callback->ref_count)
			break;

		(void)InterlockedDecrement(&device_finder_callback->ref_count);
		ref_count = device_finder_callback->ref_count;
		if (!device_finder_callback->ref_count) {
			os_free(device_finder_callback->lpVtbl);
			os_free(device_finder_callback);
		}
	} while (0);
	return ref_count;
}

static HRESULT STDMETHODCALLTYPE
device_finder_callback_device_added(IUPnPDeviceFinderCallback *This, LONG lFindData, IUPnPDevice *pDevice)
{
	HRESULT hr = S_FALSE;
	struct upnp_device_finder_callback *device_finder_callback =
			(struct upnp_device_finder_callback *)This;

	do {
		if (upnp_wps_ctrlpt_add_device(device_finder_callback->sm, pDevice))
			break;

		hr = S_OK;
	} while (0);

	return hr;
}

static HRESULT STDMETHODCALLTYPE
device_finder_callback_device_removed(IUPnPDeviceFinderCallback *This, LONG lFindData, BSTR bstrUDN)
{
	HRESULT hr = S_FALSE;
	struct upnp_device_finder_callback *device_finder_callback =
			(struct upnp_device_finder_callback *)This;
	char *udn = 0;

	do {
		if (convert_bstr_to_char(bstrUDN, &udn))
			break;

		if (upnp_wps_ctrlpt_remove_device(device_finder_callback->sm, udn))
			break;

		hr = S_OK;
	} while (0);

	if (udn)
		os_free(udn);

	return hr;
}

static HRESULT STDMETHODCALLTYPE
device_finder_callback_search_complete(IUPnPDeviceFinderCallback *This, LONG lFindData)
{
	return S_OK;
}

static int
destroy_device_finder_callback(struct upnp_wps_ctrlpt_sm *sm,
							   struct upnp_device_finder_callback *device_finder_callback)
{
	int ret = -1;

	do {
		if (!device_finder_callback)
			break;

		IUPnPDeviceFinderCallback_Release((IUPnPDeviceFinderCallback*)device_finder_callback);

		if (sm->device_finder_callback == device_finder_callback)
			sm->device_finder_callback = 0;

		ret = 0;
	} while (0);

	return ret;
}

							   
static struct upnp_device_finder_callback *
create_device_finder_callback(struct upnp_wps_ctrlpt_sm *sm)
{
	int ret = -1;
	struct upnp_device_finder_callback *device_finder_callback = 0;

	do {
		if (!sm)
			break;

		device_finder_callback = wpa_zalloc(sizeof(*device_finder_callback));
		device_finder_callback->sm = sm;
		device_finder_callback->lpVtbl= wpa_zalloc(sizeof(*device_finder_callback->lpVtbl));
		if (!device_finder_callback->lpVtbl)
			break;
		device_finder_callback->lpVtbl->QueryInterface = device_finder_callback_query_interface;
		device_finder_callback->lpVtbl->AddRef = device_finder_callback_add_ref;
		device_finder_callback->lpVtbl->Release = device_finder_callback_release;
		device_finder_callback->lpVtbl->DeviceAdded = device_finder_callback_device_added;
		device_finder_callback->lpVtbl->DeviceRemoved = device_finder_callback_device_removed;
		device_finder_callback->lpVtbl->SearchComplete = device_finder_callback_search_complete;

		IUPnPDeviceFinderCallback_AddRef((IUPnPDeviceFinderCallback *)device_finder_callback);

		ret = 0;
	} while (0);

	if (ret) {
		destroy_device_finder_callback(sm, device_finder_callback);
		device_finder_callback = 0;
	}

	return device_finder_callback;
}


struct upnp_wps_ctrlpt_sm *
upnp_wps_ctrlpt_init(struct upnp_wps_ctrlpt_ctx *ctx, void *priv)
{
	int ret = 0;
	struct upnp_wps_ctrlpt_sm *sm = 0;
    HRESULT hr = S_OK;

	do {
		sm = wpa_zalloc(sizeof(*sm));
		if (!sm)
			break;
		sm->ctx = ctx;
		sm->priv = priv;

		do {
			hr = CoInitializeEx(0, COINIT_MULTITHREADED);
			if (S_OK != hr) {
				if (S_FALSE != hr)
					break;
			} else {
				sm->co_initialized = 1;

				// Set general COM security levels
				hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
					 RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_SECURE_REFS, NULL);
				if (S_OK != hr)
					break;
			}

			ret = 1;
		} while (0);
	} while (0);

	if (!ret) {
		upnp_wps_ctrlpt_deinit(sm);
		sm = 0;
	}

	return sm;
}


void
upnp_wps_ctrlpt_deinit(struct upnp_wps_ctrlpt_sm *sm)
{
	do {
		if (!sm)
			break;

		if (sm->started)
			upnp_wps_ctrlpt_stop(sm);

		if (sm->co_initialized) {
		    CoUninitialize();
			sm->co_initialized = 0;
		}

		if (sm->device_finder_callback)
			(void)destroy_device_finder_callback(sm, sm->device_finder_callback);

		free(sm->ctx);
		free(sm);
	} while (0);
}


static int
upnp_wps_ctrlpt_delete_node(struct upnp_wps_ctrlpt_sm *sm,
							struct wps_device_node *node)
{
	int ret = -1;

	do {
		if (!sm || !node)
			break;

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

		(void)destroy_service_callback(sm, &node->device, node->device.service_callback);

		if (node->device.service)
			IUPnPService_Release(node->device.service);
		if (node->device.services)
			IUPnPServices_Release(node->device.services);
		if (node->device.handle)
			IUPnPDevice_Release(node->device.handle);

		free(node);
		node = 0;

		ret = 0;
	} while (0);

	return ret;
}


int
upnp_wps_ctrlpt_remove_device(struct upnp_wps_ctrlpt_sm *sm,
							  char *udn)
{
	int ret = -1;
	struct wps_device_node *cur, *prev = 0;

	do {
		if (!udn)
			break;

		WaitForSingleObject(sm->mutex_devlist, INFINITE);

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

		ReleaseMutex(sm->mutex_devlist);

		ret = 0;
	} while (0);

	return ret;
}


static int
upnp_wps_ctrlpt_remove_all_device(struct upnp_wps_ctrlpt_sm *sm)
{
	struct wps_device_node *cur, *next;

	WaitForSingleObject(sm->mutex_devlist, INFINITE);

	cur = sm->device_list;
	while (cur) {
		next = cur->next;
		upnp_wps_ctrlpt_delete_node(sm, cur);
		cur = next;
	}
	sm->device_list = 0;

	ReleaseMutex(sm->mutex_devlist);

	return 0;
}


int
upnp_wps_ctrlpt_add_device(struct upnp_wps_ctrlpt_sm *sm, IUPnPDevice *device)
{
	int ret = -1;
	struct wps_device_node *node = 0, *prev, *cur, *next;
	IUPnPDeviceDocumentAccess *docaccess = 0;
	BSTR service_id = 0, tmp = 0;
	char *udn = 0;

	if (!sm)
		return ret;

	WaitForSingleObject(sm->mutex_devlist, INFINITE);

	do {
		if (!SUCCEEDED(IUPnPDevice_get_UniqueDeviceName(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &udn);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!udn)
			break;

		prev = next = 0;
		cur = sm->device_list;
		while (cur) {
			next = cur->next;
			if (!os_strcmp(cur->device.udn, udn)) {
				(void)upnp_wps_ctrlpt_delete_node(sm, cur);
				break;
			}
			prev = cur;
			cur = cur->next;
		}

		node = (struct wps_device_node *)wpa_zalloc(sizeof(*node));
		if (!node)
			break;

		if (!SUCCEEDED(IUPnPDevice_get_FriendlyName(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.friendly_name);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_ManufacturerName(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.manufacturer);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_ManufacturerURL(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.manufacturer_url);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_Description(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.model_desc);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_ModelName(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.model_name);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_ModelNumber(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.model_number);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_ModelURL(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.model_url);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_SerialNumber(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.serial_number);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_UPC(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.upc);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_get_PresentationURL(device, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.pres_url);
		if (tmp) SysFreeString(tmp); tmp = 0;

		if (!SUCCEEDED(IUPnPDevice_QueryInterface(device, &IID_IUPnPDeviceDocumentAccess, (void **)&docaccess)))
			break;
		if (!SUCCEEDED(IUPnPDeviceDocumentAccess_GetDocumentURL(docaccess, &tmp)))
			break;
		(void)convert_bstr_to_char(tmp, &node->device.control_url);
		if (tmp) SysFreeString(tmp); tmp = 0;

		service_id = SysAllocString(wps_service_id);
		if (!service_id)
			break;

		if (!SUCCEEDED(IUPnPDevice_get_Services(device, &node->device.services)))
			break;

		if (!SUCCEEDED(IUPnPServices_get_Item(node->device.services, service_id, &node->device.service)))
			break;

		node->device.service_callback = create_service_callback(sm, &node->device);
		if (!node->device.service_callback)
			break;

		if (!SUCCEEDED(IUPnPService_AddCallback(node->device.service, (IUnknown *)node->device.service_callback)))
			break;

		node->device.udn = udn;
		node->device.handle = device;
		IUPnPDevice_AddRef(device);

		if (!cur) {
			if (sm->device_list) {
				next = sm->device_list;
				while(next->next)
					next = next->next;
				next->next = node;
			} else
				sm->device_list = node;
		} else if (!prev) {
			sm->device_list = node;
			sm->device_list->next = next;
		} else {
			prev->next = cur;
			cur->next = next;
		}

		ret = 0;
    } while (0);

	if (ret) {
		if (node)
			upnp_wps_ctrlpt_delete_node(sm, node);
		if (udn)
			os_free(udn);
	}

	ReleaseMutex(sm->mutex_devlist);

	if (docaccess) IUPnPDeviceDocumentAccess_Release(docaccess);
	if (service_id) SysFreeString(service_id);
	if (tmp) SysFreeString(tmp);

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

		WaitForSingleObject(sm->mutex_devlist, INFINITE);

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

			os_snprintf(cur->device.manufacturer, sizeof(cur->device.manufacturer), "%s", node->device.manufacturer);
			os_snprintf(cur->device.model_name, sizeof(cur->device.model_name), "%s", node->device.model_name);
			os_snprintf(cur->device.model_number, sizeof(cur->device.model_number), "%s", node->device.model_number);
			os_snprintf(cur->device.serial_number, sizeof(cur->device.serial_number), "%s", node->device.serial_number);
			os_snprintf(cur->device.udn, sizeof(cur->device.udn), "%s", node->device.udn);
			os_snprintf(cur->device.control_url, sizeof(cur->device.control_url), "%s", node->device.control_url);

			node = node->next;
		}

		ReleaseMutex(sm->mutex_devlist);

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
	BSTR device_type = 0;


	do {
		if (!sm)
			break;

		if (sm->device_finder) {
			if (sm->async_finder) {
				IUPnPDeviceFinder_CancelAsyncFind(sm->device_finder, sm->async_finder);
				sm->async_finder = 0;
			}

			IUPnPDeviceFinder_Release(sm->device_finder);
			sm->device_finder = 0;
		}

		if (sm->device_finder_callback)
			(void)destroy_device_finder_callback(sm, sm->device_finder_callback);

		(void)upnp_wps_ctrlpt_remove_all_device(sm);

		sm->device_finder_callback = create_device_finder_callback(sm);
		if (!sm->device_finder_callback)
			break;

        if(!SUCCEEDED(CoCreateInstance(&CLSID_UPnPDeviceFinder, NULL, CLSCTX_INPROC_SERVER, &IID_IUPnPDeviceFinder, &sm->device_finder)))
			break;

		device_type = SysAllocString(wps_device_type);
		if (!device_type)
			break;

		if (!SUCCEEDED(IUPnPDeviceFinder_CreateAsyncFind(sm->device_finder, device_type, 0, (IUnknown *)sm->device_finder_callback, &sm->async_finder)))
			break;

		if (!SUCCEEDED(IUPnPDeviceFinder_StartAsyncFind(sm->device_finder, sm->async_finder))) {
			IUPnPDeviceFinder_CancelAsyncFind(sm->device_finder, sm->async_finder);
			sm->async_finder = 0;
			IUPnPDeviceFinder_Release(sm->device_finder);
			sm->device_finder = 0;
			break;
		}

		ret = 0;
	} while (0);

	if (device_type) SysFreeString(device_type);

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
			if (!os_strcmp(next->device.control_url, control_url)) {
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

void
upnp_wps_ctrlpt_handle_event_received(struct upnp_wps_ctrlpt_sm *sm,
									  struct wps_device *device,
									  char *var_name,
									  void *value,
									  size_t value_length)
{
	do {
		if (!sm || !device || !sm->ctx || !var_name || !value)
			break;

		if (!os_strcmp("WLANEvent", var_name)) {
			int event_type;
			char event_mac[18];

			if (sm->ctx->received_wlan_event) {

				event_type = (int)*(u8*)value;
				os_strncpy(event_mac, (char *)value + 1, sizeof(event_mac) - 1);
				event_mac[sizeof(event_mac) - 1] = 0;

				sm->ctx->received_wlan_event(sm->priv,
											 device->control_url,
											 event_type,
											 event_mac,
											 (u8*)value + 18,
											 value_length - 18);
			}
		} else if (!os_strcmp("APStatus", var_name)) {
			if (sm->ctx->received_ap_status)
				sm->ctx->received_ap_status(sm->priv,
											device->control_url,
											*(u8*)value);
		} else if (!os_strcmp("STAStatus", var_name)) {
			if (sm->ctx->received_sta_status)
				sm->ctx->received_sta_status(sm->priv,
											 device->control_url,
											 *(u8*)value);
		}
	} while (0);
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


int
upnp_wps_ctrlpt_send_get_device_info(struct upnp_wps_ctrlpt_sm *sm,
									 char *control_url)
{
	int ret = -1;
	struct wps_device_node *node;
	BSTR action = 0;
	VARIANT varInArgs, varOutArgs, varReturnVal, var;
	SAFEARRAYBOUND rgsaBound[1];
	SAFEARRAY *psa = 0;
	LONG rgIndices[1];
	u8 *resp = 0;
	size_t resp_len, lbound, ubound;

	VariantInit(&varInArgs);
	VariantInit(&varOutArgs);
	VariantInit(&varReturnVal);
	VariantInit(&var);

	do {
		if (!sm || !control_url)
			break;

		if (upnp_wps_ctrlpt_get_device(sm, control_url, &node))
			break;

		if (convert_char_to_bstr("GetDeviceInfo", &action))
			break;

		rgsaBound[0].lLbound = 0;
		rgsaBound[0].cElements = 0;
		psa = SafeArrayCreate(VT_VARIANT, 1, rgsaBound);
		if (!psa)
			break;

		V_VT(&varInArgs) = VT_VARIANT|VT_ARRAY;
		V_ARRAY(&varInArgs) = psa;

		if (!SUCCEEDED(IUPnPService_InvokeAction(node->device.service, action,
					   varInArgs, &varOutArgs, &varReturnVal)))
			break;

		if (!sm->ctx->received_resp_get_device_info) {
			ret = 0;
			break;
		}

		/* NewDeviceInfo */
		rgIndices[0] = 0;
		if (!SUCCEEDED(SafeArrayGetElement(V_ARRAY(&varOutArgs), rgIndices, (void *)&var)))
			break;

		ubound = 0;
		if (!SUCCEEDED(SafeArrayGetUBound(V_ARRAY(&var), 1, (LONG *)&ubound)))
			break;

		lbound = 0;
		if (!SUCCEEDED(SafeArrayGetLBound(V_ARRAY(&var), 1, (LONG *)&lbound)))
			break;

		resp_len = ubound - lbound + 1;

		if (!SUCCEEDED(SafeArrayAccessData(V_ARRAY(&var), &resp)))
			break;

		ret = sm->ctx->received_resp_get_device_info(sm->priv, control_url, resp, resp_len);

		(void)SafeArrayUnaccessData(V_ARRAY(&var));
	} while (0);

	VariantClear(&varInArgs);
	VariantClear(&varReturnVal);
	VariantClear(&varOutArgs);

	if (action) SysFreeString(action);

	return ret;
}


int
upnp_wps_ctrlpt_send_put_message(struct upnp_wps_ctrlpt_sm *sm,
								 char *control_url,
								 u8 *msg, size_t msg_len)
{
	int ret = -1;
	struct wps_device_node *node;
	BSTR action = 0;
	VARIANT varInArgs, varOutArgs, varReturnVal, var, var2;
	SAFEARRAYBOUND rgsaBound[1];
	SAFEARRAY *psa = 0, *val = 0;
	LONG rgIndices[1];
	u8 *resp = 0;
	size_t resp_len, ubound, lbound;
	size_t i;

	VariantInit(&varInArgs);
	VariantInit(&varOutArgs);
	VariantInit(&varReturnVal);
	VariantInit(&var);
	VariantInit(&var2);

	do {
		if (!sm || !control_url || !msg)
			break;

		if (upnp_wps_ctrlpt_get_device(sm, control_url, &node))
			break;

		if (convert_char_to_bstr("PutMessage", &action))
			break;

		rgsaBound[0].lLbound = 0;
		rgsaBound[0].cElements = 1;
		psa = SafeArrayCreate(VT_VARIANT, 1, rgsaBound);
		if (!psa)
			break;

		rgsaBound[0].lLbound = 0;
		rgsaBound[0].cElements = msg_len;
		val = SafeArrayCreate(VT_UI1, 1, rgsaBound);
		if (!val)
			break;

		for (i = 0; i < msg_len; i++) {
			rgIndices[0] = i;
			if (!SUCCEEDED(SafeArrayPutElement(val, rgIndices, (void *)(msg + i))))
				break;
		}
		if (i != msg_len)
			break;

		/* NewInMessage */
		V_VT(&var) = VT_ARRAY|VT_UI1;
		V_ARRAY(&var) = val;

		rgIndices[0] = 0;
		if (!SUCCEEDED(SafeArrayPutElement(psa, rgIndices, (void *)&var)))
			break;

		V_VT(&varInArgs) = VT_VARIANT|VT_ARRAY;
		V_ARRAY(&varInArgs) = psa;

		if (!SUCCEEDED(IUPnPService_InvokeAction(node->device.service, action,
					   varInArgs, &varOutArgs, &varReturnVal)))
			break;

		if (!sm->ctx->received_resp_put_message) {
			ret = 0;
			break;
		}

		/* NewOutMessage */
		rgIndices[0] = 0;
		if (!SUCCEEDED(SafeArrayGetElement(V_ARRAY(&varOutArgs), rgIndices, (void *)&var2)))
			break;

		ubound = 0;
		if (!SUCCEEDED(SafeArrayGetUBound(V_ARRAY(&var2), 1, (LONG *)&ubound)))
			break;

		lbound = 0;
		if (!SUCCEEDED(SafeArrayGetLBound(V_ARRAY(&var2), 1, (LONG *)&lbound)))
			break;

		resp_len = ubound - lbound + 1;

		if (!SUCCEEDED(SafeArrayAccessData(V_ARRAY(&var2), &resp)))
			break;

		ret = sm->ctx->received_resp_put_message(sm->priv, control_url, resp, resp_len);

		(void)SafeArrayUnaccessData(V_ARRAY(&var2));
	} while (0);

	VariantClear(&varInArgs);
	VariantClear(&varReturnVal);
	VariantClear(&varOutArgs);

	if (action) SysFreeString(action);

	return ret;
}


int
upnp_wps_ctrlpt_send_put_wlan_response(struct upnp_wps_ctrlpt_sm *sm,
									   char *control_url, int ev_type,
									   u8 *mac, u8 *msg, size_t msg_len)
{
	int ret = -1;
	struct wps_device_node *node;
	BSTR action = 0;
	VARIANT varInArgs, varOutArgs, varReturnVal, var1, var2, var3;
	SAFEARRAYBOUND rgsaBound[1];
	SAFEARRAY *psa = 0, *val = 0;
	LONG rgIndices[1];
	char mac_address[18];
	size_t i;
	BSTR bstrMac = 0;

	VariantInit(&varInArgs);
	VariantInit(&varOutArgs);
	VariantInit(&varReturnVal);
	VariantInit(&var1);
	VariantInit(&var2);
	VariantInit(&var3);

	do {
		if (!sm || !control_url || !mac || !msg)
			break;

		if (upnp_wps_ctrlpt_get_device(sm, control_url, &node))
			break;

		if (convert_char_to_bstr("PutWLANResponse", &action))
			break;

		os_snprintf(mac_address, sizeof(mac_address), "%02X:%02X:%02X:%02X:%02X",
					mac[1], mac[2], mac[3], mac[4], mac[5], mac[6]);
		if (convert_char_to_bstr(mac_address, &bstrMac))
			break;

		rgsaBound[0].lLbound = 0;
		rgsaBound[0].cElements = 3;
		psa = SafeArrayCreate(VT_VARIANT, 1, rgsaBound);
		if (!psa)
			break;

		rgsaBound[0].lLbound = 0;
		rgsaBound[0].cElements = msg_len;
		val = SafeArrayCreate(VT_UI1, 1, rgsaBound);
		if (!val)
			break;

		for (i = 0; i < msg_len; i++) {
			rgIndices[0] = i;
			if (!SUCCEEDED(SafeArrayPutElement(val, rgIndices, (void *)(msg + i))))
				break;
		}
		if (i != msg_len)
			break;

		/* NewMessage */
		V_VT(&var1) = VT_ARRAY|VT_UI1;
		V_ARRAY(&var1) = val;

		rgIndices[0] = 0;
		if (!SUCCEEDED(SafeArrayPutElement(psa, rgIndices, (void *)&var1)))
			break;

		/* NewWLANEventType */
		V_VT(&var2) = VT_UI1;
		V_UI1(&var2) = ev_type & (UPNP_WPS_WLANEVENT_TYPE_PROBE|UPNP_WPS_WLANEVENT_TYPE_EAP);

		rgIndices[0] = 1;
		if (!SUCCEEDED(SafeArrayPutElement(psa, rgIndices, (void *)&var2)))
			break;

		/* NewWLANEventMAC */
		V_VT(&var3) = VT_BSTR;
		V_BSTR(&var3) = bstrMac;

		rgIndices[0] = 2;
		if (!SUCCEEDED(SafeArrayPutElement(psa, rgIndices, (void *)&var3)))
			break;

		V_VT(&varInArgs) = VT_VARIANT|VT_ARRAY;
		V_ARRAY(&varInArgs) = psa;

		if (!SUCCEEDED(IUPnPService_InvokeAction(node->device.service, action,
					   varInArgs, &varOutArgs, &varReturnVal)))
			break;

		ret = 0;
	} while (0);

	VariantClear(&varInArgs);
	VariantClear(&varOutArgs);
	VariantClear(&varReturnVal);

	if (action) SysFreeString(action);
	if (bstrMac) SysFreeString(bstrMac);

	return ret;
}

int
upnp_wps_ctrlpt_send_set_selected_registrar(struct upnp_wps_ctrlpt_sm *sm,
											char *control_url,
											u8 *msg, size_t msg_len)
{
	int ret = -1;
	struct wps_device_node *node;
	BSTR action = 0;
	VARIANT varInArgs, varOutArgs, varReturnVal, var;
	SAFEARRAYBOUND rgsaBound[1];
	SAFEARRAY *psa = 0, *val = 0;
	LONG rgIndices[1];
	size_t i;

	VariantInit(&varInArgs);
	VariantInit(&varOutArgs);
	VariantInit(&varReturnVal);
	VariantInit(&var);

	do {
		if (!sm || !control_url || !msg)
			break;

		if (upnp_wps_ctrlpt_get_device(sm, control_url, &node))
			break;

		if (convert_char_to_bstr("SetSelectedRegistrar", &action))
			break;

		rgsaBound[0].lLbound = 0;
		rgsaBound[0].cElements = 1;
		psa = SafeArrayCreate(VT_VARIANT, 1, rgsaBound);
		if (!psa)
			break;

		rgsaBound[0].lLbound = 0;
		rgsaBound[0].cElements = msg_len;
		val = SafeArrayCreate(VT_UI1, 1, rgsaBound);
		if (!val)
			break;

		for (i = 0; i < msg_len; i++) {
			rgIndices[0] = i;
			if (!SUCCEEDED(SafeArrayPutElement(val, rgIndices, (void *)(msg + i))))
				break;
		}
		if (i != msg_len)
			break;

		/* NewMessage */
		V_VT(&var) = VT_ARRAY|VT_UI1;
		V_ARRAY(&var) = val;

		rgIndices[0] = 0;
		if (!SUCCEEDED(SafeArrayPutElement(psa, rgIndices, (void *)&var)))
			break;

		V_VT(&varInArgs) = VT_VARIANT|VT_ARRAY;
		V_ARRAY(&varInArgs) = psa;

		if (!SUCCEEDED(IUPnPService_InvokeAction(node->device.service, action, varInArgs, &varOutArgs, &varReturnVal)))
			break;

		ret = 0;
	} while (0);

	VariantClear(&varInArgs);
	VariantClear(&varOutArgs);
	VariantClear(&varReturnVal);

	if (action) SysFreeString(action);

	return ret;
}

int
upnp_wps_ctrlpt_start(struct upnp_wps_ctrlpt_sm *sm, char *net_if)
{
	int ret = -1;

	do {
		if (!sm)
			break;

		if (sm->started)
			upnp_wps_ctrlpt_stop(sm);

		sm->mutex_devlist = CreateMutex(0, 0, 0);

		sm->started = 1;

		(void)upnp_wps_ctrlpt_refresh_device(sm, 0);

		ret = 0;
	} while (0);

	return ret;
}


int
upnp_wps_ctrlpt_stop(struct upnp_wps_ctrlpt_sm *sm)
{
	do {
		if (!sm)
			break;

		if (!sm->started)
			break;

		if (sm->mutex_devlist)
			WaitForSingleObject(sm->mutex_devlist, INFINITE);

		if (sm->device_finder) {
			if (sm->async_finder) {
				IUPnPDeviceFinder_CancelAsyncFind(sm->device_finder, sm->async_finder);
				sm->async_finder = 0;
			}

			IUPnPDeviceFinder_Release(sm->device_finder);
			sm->device_finder = 0;
		}

		if (sm->device_finder_callback)
			(void)destroy_device_finder_callback(sm, sm->device_finder_callback);

		if (sm->mutex_devlist)
			ReleaseMutex(sm->mutex_devlist);

		(void)upnp_wps_ctrlpt_remove_all_device(sm);

		if (sm->mutex_devlist) {
			CloseHandle(sm->mutex_devlist);
			sm->mutex_devlist = 0;
		}
	} while (0);

	if (sm)
		sm->started = 0;
	return 0;
}


