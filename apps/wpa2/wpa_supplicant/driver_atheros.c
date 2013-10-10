/*
 * WPA Supplicant - driver interaction with MADWIFI 802.11 driver
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004-2005, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>

#include "common.h"
#include "driver.h"
#include "driver_wext.h"
#include "eloop.h"
#include "wpa_supplicant.h"
#include "wpa.h"
#include "wireless_copy.h"

#include <include/compat.h>

#include "os/linux/include/ieee80211_external.h"

#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
#include "wps_config.h"
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

struct wpa_driver_atheros_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
#ifdef CONFIG_ETHERSTA
    unsigned char own_addr[6];
#endif /* CONFIG_ETHERSTA */
};

static int
set80211priv(struct wpa_driver_atheros_data *drv, int op, void *data, int len,
	     int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	if (len < IFNAMSIZ) {
		/*
		 * Argument data fits inline; put it there.
		 */
#ifdef EAP_WPS
#ifdef ATH_WPS_IE
		if (op == IEEE80211_IOCTL_SET_APPIEBUF) {
			iwr.u.data.pointer = data;
			iwr.u.data.length = len;
		} else
#endif /* ATH_WPS_IE */
#endif /* EAP_WPS */
		os_memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(drv->sock, op, &iwr) < 0) {
		if (show_err) {
#ifdef MADWIFI_NG
			int first = IEEE80211_IOCTL_SETPARAM;
			/* NOT NEEDED: int last = IEEE80211_IOCTL_KICKMAC; */
			static const char *opnames[] = {
				"ioctl[IEEE80211_IOCTL_SETPARAM]",
				"ioctl[IEEE80211_IOCTL_GETPARAM]",
				"ioctl[IEEE80211_IOCTL_SETMODE]",
				"ioctl[IEEE80211_IOCTL_GETMODE]",
				"ioctl[IEEE80211_IOCTL_SETWMMPARAMS]",
				"ioctl[IEEE80211_IOCTL_GETWMMPARAMS]",
				"ioctl[IEEE80211_IOCTL_SETCHANLIST]",
				"ioctl[IEEE80211_IOCTL_GETCHANLIST]",
				"ioctl[IEEE80211_IOCTL_CHANSWITCH]",
				NULL,
				NULL,
				"ioctl[IEEE80211_IOCTL_GETSCANRESULTS]",
				NULL,
				"ioctl[IEEE80211_IOCTL_GETCHANINFO]",
				"ioctl[IEEE80211_IOCTL_SETOPTIE]",
				"ioctl[IEEE80211_IOCTL_GETOPTIE]",
				"ioctl[IEEE80211_IOCTL_SETMLME]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_ADDMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_WDSMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_WDSDELMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_KICKMAC]",
			};
#else /* MADWIFI_NG */
			int first = IEEE80211_IOCTL_SETPARAM;
			/* NOT NEEDED: int last = IEEE80211_IOCTL_CHANLIST; */
			static const char *opnames[] = {
				"ioctl[IEEE80211_IOCTL_SETPARAM]",
				"ioctl[IEEE80211_IOCTL_GETPARAM]",
				"ioctl[IEEE80211_IOCTL_SETKEY]",
				"ioctl[IEEE80211_IOCTL_GETKEY]",
				"ioctl[IEEE80211_IOCTL_DELKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETMLME]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETOPTIE]",
				"ioctl[IEEE80211_IOCTL_GETOPTIE]",
				"ioctl[IEEE80211_IOCTL_ADDMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_CHANLIST]",
			};
#endif /* MADWIFI_NG */
			int idx = op - first;
			if (first <= op && /* NOT NEEDED: op <= last && */
			    idx < (int) (sizeof(opnames) / sizeof(opnames[0]))
			    && opnames[idx])
				perror(opnames[idx]);
			else
				perror("ioctl[unknown???]");
		}
		return -1;
	}
	return 0;
}

static int
set80211param(struct wpa_driver_atheros_data *drv, int op, int arg,
	      int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.mode = op;
	os_memcpy(iwr.u.name+sizeof(__u32), &arg, sizeof(arg));

	if (ioctl(drv->sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		if (show_err) 
			perror("ioctl[IEEE80211_IOCTL_SETPARAM]");
		return -1;
	}
	return 0;
}

#ifdef MODIFIED_BY_SONY
static int
get80211param(struct wpa_driver_atheros_data *drv, int op, int *arg,
	      int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.mode = op;

	if (ioctl(drv->sock, IEEE80211_IOCTL_GETPARAM, &iwr) < 0) {
		if (show_err) 
			perror("ioctl[IEEE80211_IOCTL_GETPARAM]");
		return -1;
	}
	os_memcpy(arg, &iwr.u.param.value, sizeof(arg));
	return 0;
}
#endif /* MODIFIED_BY_SONY */

static int
wpa_driver_atheros_set_wpa_ie(struct wpa_driver_atheros_data *drv,
			      const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	/* NB: SETOPTIE is not fixed-size so must not be inlined */
	iwr.u.data.pointer = (void *) wpa_ie;
	iwr.u.data.length = wpa_ie_len;

	if (ioctl(drv->sock, IEEE80211_IOCTL_SETOPTIE, &iwr) < 0) {
		perror("ioctl[IEEE80211_IOCTL_SETOPTIE]");
		return -1;
	}
	return 0;
}

static int
wpa_driver_atheros_del_key(struct wpa_driver_atheros_data *drv, int key_idx,
			   const u8 *addr)
{
	struct ieee80211req_del_key wk;

	wpa_printf(MSG_DEBUG, "%s: keyidx=%d", __FUNCTION__, key_idx);
	os_memset(&wk, 0, sizeof(wk));
	wk.idk_keyix = key_idx;
	if (addr != NULL)
		os_memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);

	return set80211priv(drv, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk), 1);
}

static int
wpa_driver_atheros_set_key(void *priv, wpa_alg alg,
			   const u8 *addr, int key_idx, int set_tx,
			   const u8 *seq, size_t seq_len,
			   const u8 *key, size_t key_len)
{
	struct wpa_driver_atheros_data *drv = priv;
	struct ieee80211req_key wk;
	char *alg_name;
	u_int8_t cipher;

	if (alg == WPA_ALG_NONE)
		return wpa_driver_atheros_del_key(drv, key_idx, addr);

	switch (alg) {
	case WPA_ALG_WEP:
		if (addr == NULL || os_memcmp(addr, "\xff\xff\xff\xff\xff\xff",
					      ETH_ALEN) == 0) {
			/*
			 * atheros did not seem to like static WEP key
			 * configuration with IEEE80211_IOCTL_SETKEY, so use
			 * Linux wireless extensions ioctl for this.
			 */
			return wpa_driver_wext_set_key(drv->wext, alg, addr,
						       key_idx, set_tx,
						       seq, seq_len,
						       key, key_len);
		}
		alg_name = "WEP";
		cipher = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
	default:
		wpa_printf(MSG_DEBUG, "%s: unknown/unsupported algorithm %d",
			__FUNCTION__, alg);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: alg=%s key_idx=%d set_tx=%d seq_len=%lu "
		   "key_len=%lu", __FUNCTION__, alg_name, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);

	if (seq_len > sizeof(u_int64_t)) {
		wpa_printf(MSG_DEBUG, "%s: seq_len %lu too big",
			   __FUNCTION__, (unsigned long) seq_len);
		return -2;
	}
	if (key_len > sizeof(wk.ik_keydata)) {
		wpa_printf(MSG_DEBUG, "%s: key length %lu too big",
			   __FUNCTION__, (unsigned long) key_len);
		return -3;
	}

	os_memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV;
	if (addr == NULL ||
	    os_memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
		wk.ik_flags |= IEEE80211_KEY_GROUP;
	if (set_tx) {
		wk.ik_flags |= IEEE80211_KEY_XMIT | IEEE80211_KEY_DEFAULT;
		os_memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	} else
		os_memset(wk.ik_macaddr, 0, IEEE80211_ADDR_LEN);
	wk.ik_keyix = key_idx;
	wk.ik_keylen = key_len;
#ifdef WORDS_BIGENDIAN
#define WPA_KEY_RSC_LEN 8
	{
		size_t i;
		u8 tmp[WPA_KEY_RSC_LEN];
		os_memset(tmp, 0, sizeof(tmp));
		for (i = 0; i < seq_len; i++)
			tmp[WPA_KEY_RSC_LEN - i - 1] = seq[i];
		os_memcpy(&wk.ik_keyrsc, tmp, WPA_KEY_RSC_LEN);
	}
#else /* WORDS_BIGENDIAN */
	os_memcpy(&wk.ik_keyrsc, seq, seq_len);
#endif /* WORDS_BIGENDIAN */
	os_memcpy(wk.ik_keydata, key, key_len);

	return set80211priv(drv, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk), 1);
}

static int
wpa_driver_atheros_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_atheros_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_COUNTERMEASURES, enabled, 1);
}


static int
wpa_driver_atheros_set_drop_unencrypted(void *priv, int enabled)
{
	struct wpa_driver_atheros_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_DROPUNENCRYPTED, enabled, 1);
}

static int
wpa_driver_atheros_deauthenticate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_atheros_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme), 1);
}

static int
wpa_driver_atheros_disassociate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_atheros_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme), 1);
}

static int
wpa_driver_atheros_associate(void *priv,
			     struct wpa_driver_associate_params *params)
{
	struct wpa_driver_atheros_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret = 0, privacy = 1;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

#ifdef MODIFIED_BY_SONY
		(void)wpa_driver_wext_auth_alg_fallback(drv->wext, params);
#endif /* MODIFIED_BY_SONY */

	/*
	 * NB: Don't need to set the freq or cipher-related state as
	 *     this is implied by the bssid which is used to locate
	 *     the scanned node state which holds it.  The ssid is
	 *     needed to disambiguate an AP that broadcasts multiple
	 *     ssid's but uses the same bssid.
	 */
	/* XXX error handling is wrong but unclear what to do... */
	if (wpa_driver_atheros_set_wpa_ie(drv, params->wpa_ie,
					  params->wpa_ie_len) < 0)
		ret = -1;

	if (params->pairwise_suite == CIPHER_NONE &&
	    params->group_suite == CIPHER_NONE &&
	    params->key_mgmt_suite == KEY_MGMT_NONE &&
	    params->wpa_ie_len == 0)
		privacy = 0;

	if (set80211param(drv, IEEE80211_PARAM_PRIVACY, privacy, 1) < 0)
		ret = -1;

	if (params->wpa_ie_len &&
	    set80211param(drv, IEEE80211_PARAM_WPA,
			  params->wpa_ie[0] == RSN_INFO_ELEM ? 2 : 1, 1) < 0)
		ret = -1;

	if (params->bssid == NULL) {
		/* ap_scan=2 mode - driver takes care of AP selection and
		 * roaming */
		/* FIX: this does not seem to work; would probably need to
		 * change something in the driver */
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 1, 1) < 0)
			ret = -1;

		if (wpa_driver_wext_set_ssid(drv->wext, params->ssid,
					     params->ssid_len) < 0)
			ret = -1;
	} else {
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0)
			ret = -1;
		if (wpa_driver_wext_set_ssid(drv->wext, params->ssid,
					     params->ssid_len) < 0)
			ret = -1;
		os_memset(&mlme, 0, sizeof(mlme));
		mlme.im_op = IEEE80211_MLME_ASSOC;
		os_memcpy(mlme.im_macaddr, params->bssid, IEEE80211_ADDR_LEN);
		if (set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme,
				 sizeof(mlme), 1) < 0) {
			wpa_printf(MSG_DEBUG, "%s: SETMLME[ASSOC] failed",
				   __func__);
			ret = -1;
		}
	}

	return ret;
}

static int
wpa_driver_atheros_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_atheros_data *drv = priv;
	int authmode;

	if ((auth_alg & AUTH_ALG_OPEN_SYSTEM) &&
	    (auth_alg & AUTH_ALG_SHARED_KEY))
		authmode = IEEE80211_AUTH_AUTO;
	else if (auth_alg & AUTH_ALG_SHARED_KEY)
		authmode = IEEE80211_AUTH_SHARED;
	else
		authmode = IEEE80211_AUTH_OPEN;

	return set80211param(drv, IEEE80211_PARAM_AUTHMODE, authmode, 1);
}

static int
wpa_driver_atheros_scan(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct wpa_driver_atheros_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

	/* set desired ssid before scan */
	/* FIX: scan should not break the current association, so using
	 * set_ssid may not be the best way of doing this.. */
	if (wpa_driver_wext_set_ssid(drv->wext, ssid, ssid_len) < 0)
		ret = -1;

	if (ioctl(drv->sock, SIOCSIWSCAN, &iwr) < 0) {
		perror("ioctl[SIOCSIWSCAN]");
		ret = -1;
	}

	/*
	 * atheros delivers a scan complete event so no need to poll, but
	 * register a backup timeout anyway to make sure that we recover even
	 * if the driver does not send this event for any reason. This timeout
	 * will only be used if the event is not delivered (event handler will
	 * cancel the timeout).
	 */
	eloop_register_timeout(30, 0, wpa_driver_wext_scan_timeout, drv->wext,
			       drv->ctx);

	return ret;
}

static int wpa_driver_atheros_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_atheros_data *drv = priv;
	return wpa_driver_wext_get_bssid(drv->wext, bssid);
}


static int wpa_driver_atheros_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_atheros_data *drv = priv;
	return wpa_driver_wext_get_ssid(drv->wext, ssid);
}


static int wpa_driver_atheros_get_scan_results(void *priv,
					    struct wpa_scan_result *results,
					    size_t max_size)
{
	struct wpa_driver_atheros_data *drv = priv;
	return wpa_driver_wext_get_scan_results(drv->wext, results, max_size);
}


static int wpa_driver_atheros_set_operstate(void *priv, int state)
{
	struct wpa_driver_atheros_data *drv = priv;
	return wpa_driver_wext_set_operstate(drv->wext, state);
}


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
static int
atheros_set_wps_ie(void *priv, u8 *iebuf, int iebuflen, u32 frametype)
{
#define IEEE80211_APPIE_MAX    1024 /* max appie buffer size */
	u8 buf[IEEE80211_APPIE_MAX];    /* TDLS_FTIE is more than 400 */
	struct ieee80211req_getset_appiebuf * beac_ie;
	// int i;

	wpa_printf(MSG_DEBUG, "%s buflen = %d\n", __func__, iebuflen);

	beac_ie = (struct ieee80211req_getset_appiebuf *) buf;
	beac_ie->app_frmtype = frametype;
	beac_ie->app_buflen = iebuflen;
	os_memcpy(&(beac_ie->app_buf[0]), iebuf, iebuflen);
	
	return set80211priv(priv, IEEE80211_IOCTL_SET_APPIEBUF, beac_ie,
			sizeof(struct ieee80211req_getset_appiebuf) + iebuflen, 1);
}


static int wpa_driver_atheros_set_wps_probe_req_ie(
			     void *priv, u8 *iebuf, int iebuflen)
{
	return atheros_set_wps_ie(priv, iebuf, iebuflen, 
			IEEE80211_APPIE_FRAME_PROBE_REQ);
}


static int wpa_driver_atheros_set_wps_assoc_req_ie(
			     void *priv, u8 *iebuf, int iebuflen)
{
	return atheros_set_wps_ie(priv, iebuf, iebuflen, 
			IEEE80211_APPIE_FRAME_ASSOC_REQ);
}
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */


static void * wpa_driver_atheros_init(void *ctx, const char *ifname)
{
	struct wpa_driver_atheros_data *drv;
#ifdef MODIFIED_BY_SONY
	int caps = 0;
#endif /* MODIFIED_BY_SONY */
#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
	int ret = -1;
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->wext = wpa_driver_wext_init(ctx, ifname);
	if (drv->wext == NULL)
		goto fail;

	drv->ctx = ctx;
	os_strncpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0)
		goto fail2;

#ifdef MODIFIED_BY_SONY
	if (get80211param(drv, IEEE80211_PARAM_DRIVER_CAPS, &caps, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to get driver capabilities"
			   "device_caps", __FUNCTION__);
	}
	wpa_printf(MSG_DEBUG, "driver capabilities : 0x%08X", caps);
#endif /* MODIFIED_BY_SONY */

	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to set wpa_supplicant-based "
			   "roaming", __FUNCTION__);
		goto fail3;
	}

	if (set80211param(drv, IEEE80211_PARAM_WPA, 3, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable WPA support",
			   __FUNCTION__);
		goto fail3;
	}

#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
	do {
		/* Clear WPS ProbeResp IE */
		if (wpa_driver_atheros_set_wps_probe_req_ie(drv, 0, 0)) {
			wpa_printf(MSG_DEBUG, "%s: failed to clear WPS ProbeReq IE",
				   __FUNCTION__);
			break;
		}
		/* Clear WPS AssocReq IE */
		if (wpa_driver_atheros_set_wps_assoc_req_ie(drv, 0, 0)) {
			wpa_printf(MSG_DEBUG, "%s: failed to clear WPS AssocReq IE",
				   __FUNCTION__);
			break;
		}
		ret = 0;
	} while (0);

	if (ret)
		goto fail3;
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

	return drv;

fail3:
	close(drv->sock);
fail2:
	wpa_driver_wext_deinit(drv->wext);
fail:
	os_free(drv);
	return NULL;
}


static void wpa_driver_atheros_deinit(void *priv)
{
	struct wpa_driver_atheros_data *drv = priv;

	if (wpa_driver_atheros_set_wpa_ie(drv, NULL, 0) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to clear WPA IE",
			   __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable driver-based "
			   "roaming", __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_PRIVACY, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to disable forced Privacy "
			   "flag", __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_WPA, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to disable WPA",
			   __FUNCTION__);
	}

	wpa_driver_wext_deinit(drv->wext);

	close(drv->sock);
	os_free(drv);
}

#ifdef CONFIG_ETHERSTA
/* This function is only used by ether-dongle mode */
static u8*
wpa_driver_atheros_get_mac_addr(void *priv)
{
	struct wpa_driver_atheros_data *drv = priv;
	struct iwreq iwr;
	char mac[6];

	wpa_printf(MSG_DEBUG, "%s: new: come to here\n", __func__);
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

	iwr.u.data.pointer = (void*) mac;
	iwr.u.data.length = sizeof(mac);

	if ( ioctl(drv->sock, IEEE80211_IOCTL_GETMAC, &iwr) < 0 )
	{
		return NULL;
	}

	memcpy(drv->own_addr, mac, sizeof(mac));

	return drv->own_addr;
}
#endif /* CONFIG_ETHERSTA */


#ifdef CONFIG_TDLS
int wpa_driver_atheros_send_tdls_mgmt(void *priv, u8* buf, u16 len)
{
    return atheros_set_wps_ie(priv, buf, len, IEEE80211_APPIE_FRAME_TDLS_FTIE);
}
#endif /* CONFIG_TDLS */

const struct wpa_driver_ops wpa_driver_atheros_ops = {
	.name			= "atheros",
	.desc			= "Atheros Native Driver support",
	.get_bssid		= wpa_driver_atheros_get_bssid,
	.get_ssid		= wpa_driver_atheros_get_ssid,
	.set_key		= wpa_driver_atheros_set_key,
	.init			= wpa_driver_atheros_init,
	.deinit			= wpa_driver_atheros_deinit,
	.set_countermeasures	= wpa_driver_atheros_set_countermeasures,
	.set_drop_unencrypted	= wpa_driver_atheros_set_drop_unencrypted,
	.scan			= wpa_driver_atheros_scan,
	.get_scan_results	= wpa_driver_atheros_get_scan_results,
	.deauthenticate		= wpa_driver_atheros_deauthenticate,
	.disassociate		= wpa_driver_atheros_disassociate,
	.associate		= wpa_driver_atheros_associate,
	.set_auth_alg		= wpa_driver_atheros_set_auth_alg,
	.set_operstate		= wpa_driver_atheros_set_operstate,
#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
	.set_wps_probe_req_ie	= wpa_driver_atheros_set_wps_probe_req_ie,
	.set_wps_assoc_req_ie	= wpa_driver_atheros_set_wps_assoc_req_ie,
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */
#ifdef CONFIG_ETHERSTA
    .get_mac_addr           = wpa_driver_atheros_get_mac_addr,
#endif /* CONFIG_ETHERSTA */
#ifdef CONFIG_TDLS
    .send_tdls_mgmt         = wpa_driver_atheros_send_tdls_mgmt,
#endif /* CONFIG_TDLS */
};
