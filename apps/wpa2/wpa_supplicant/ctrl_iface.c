/*
 * WPA Supplicant / Control interface (shared code for all backends)
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
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

#include "common.h"
#include "eloop.h"
#include "wpa.h"
#include "wpa_supplicant.h"
#include "config.h"
#include "eapol_sm.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface.h"
#include "l2_packet.h"
#include "preauth.h"
#include "pmksa_cache.h"
#include "wpa_ctrl.h"
#include "eap.h"

#ifdef EAP_WPS
#include "wps_config.h"
#include "wps_parser.h"
#ifndef USE_INTEL_SDK
#include "eap_wps.h"
#endif /* USE_INTEL_SDK */
#ifdef WPS_OPT_UPNP
#include "upnp_wps_ctrlpt.h"
#include "wps_opt_upnp.h"
#endif /* WPS_OPT_UPNP */
#ifdef WPS_OPT_NFC
#include "wps_opt_nfc.h"
#endif /* WPS_OPT_NFC */
#endif /* EAP_WPS */


static int wpa_supplicant_global_iface_interfaces(struct wpa_global *global,
						  char *buf, int len);


static int wpa_supplicant_ctrl_iface_set(struct wpa_supplicant *wpa_s,
					 char *cmd)
{
	char *value;
	int ret = 0;

	value = os_strchr(cmd, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET '%s'='%s'", cmd, value);
	if (os_strcasecmp(cmd, "EAPOL::heldPeriod") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   atoi(value), -1, -1, -1);
	} else if (os_strcasecmp(cmd, "EAPOL::authPeriod") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   -1, atoi(value), -1, -1);
	} else if (os_strcasecmp(cmd, "EAPOL::startPeriod") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   -1, -1, atoi(value), -1);
	} else if (os_strcasecmp(cmd, "EAPOL::maxStart") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   -1, -1, -1, atoi(value));
	} else if (os_strcasecmp(cmd, "dot11RSNAConfigPMKLifetime") == 0) {
		if (wpa_sm_set_param(wpa_s->wpa, RSNA_PMK_LIFETIME,
				     atoi(value)))
			ret = -1;
	} else if (os_strcasecmp(cmd, "dot11RSNAConfigPMKReauthThreshold") ==
		   0) {
		if (wpa_sm_set_param(wpa_s->wpa, RSNA_PMK_REAUTH_THRESHOLD,
				     atoi(value)))
			ret = -1;
	} else if (os_strcasecmp(cmd, "dot11RSNAConfigSATimeout") == 0) {
		if (wpa_sm_set_param(wpa_s->wpa, RSNA_SA_TIMEOUT, atoi(value)))
			ret = -1;
	} else
		ret = -1;

	return ret;
}


static int wpa_supplicant_ctrl_iface_preauth(struct wpa_supplicant *wpa_s,
					     char *addr)
{
	u8 bssid[ETH_ALEN];

	if (hwaddr_aton(addr, bssid)) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE PREAUTH: invalid address "
			   "'%s'", addr);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "CTRL_IFACE PREAUTH " MACSTR, MAC2STR(bssid));
	rsn_preauth_deinit(wpa_s->wpa);
	if (rsn_preauth_init(wpa_s->wpa, bssid, wpa_s->current_ssid))
		return -1;

	return 0;
}


#ifdef CONFIG_PEERKEY
/* MLME-STKSTART.request(peer) */
static int wpa_supplicant_ctrl_iface_stkstart(
	struct wpa_supplicant *wpa_s, char *addr)
{
	u8 peer[ETH_ALEN];

	if (hwaddr_aton(addr, peer)) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE STKSTART: invalid "
			   "address '%s'", peer);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "CTRL_IFACE STKSTART " MACSTR,
		   MAC2STR(peer));

	return wpa_sm_stkstart(wpa_s->wpa, peer);
}
#endif /* CONFIG_PEERKEY */


static int wpa_supplicant_ctrl_iface_ctrl_rsp(struct wpa_supplicant *wpa_s,
					      char *rsp)
{
#ifdef IEEE8021X_EAPOL
	char *pos, *id_pos;
	int id;
	struct wpa_ssid *ssid;

	pos = os_strchr(rsp, '-');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';
	id_pos = pos;
	pos = os_strchr(pos, ':');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';
	id = atoi(id_pos);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: field=%s id=%d", rsp, id);
	wpa_hexdump_ascii_key(MSG_DEBUG, "CTRL_IFACE: value",
			      (u8 *) pos, os_strlen(pos));

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find SSID id=%d "
			   "to update", id);
		return -1;
	}

	if (os_strcmp(rsp, "IDENTITY") == 0) {
		os_free(ssid->identity);
		ssid->identity = (u8 *) os_strdup(pos);
		ssid->identity_len = os_strlen(pos);
		ssid->pending_req_identity = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (os_strcmp(rsp, "PASSWORD") == 0) {
		os_free(ssid->password);
		ssid->password = (u8 *) os_strdup(pos);
		ssid->password_len = os_strlen(pos);
		ssid->pending_req_password = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (os_strcmp(rsp, "NEW_PASSWORD") == 0) {
		os_free(ssid->new_password);
		ssid->new_password = (u8 *) os_strdup(pos);
		ssid->new_password_len = os_strlen(pos);
		ssid->pending_req_new_password = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (os_strcmp(rsp, "PIN") == 0) {
		os_free(ssid->pin);
		ssid->pin = os_strdup(pos);
		ssid->pending_req_pin = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (os_strcmp(rsp, "OTP") == 0) {
		os_free(ssid->otp);
		ssid->otp = (u8 *) os_strdup(pos);
		ssid->otp_len = os_strlen(pos);
		os_free(ssid->pending_req_otp);
		ssid->pending_req_otp = NULL;
		ssid->pending_req_otp_len = 0;
	} else if (os_strcmp(rsp, "PASSPHRASE") == 0) {
		os_free(ssid->private_key_passwd);
		ssid->private_key_passwd = (u8 *) os_strdup(pos);
		ssid->pending_req_passphrase = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Unknown field '%s'", rsp);
		return -1;
	}

	return 0;
#else /* IEEE8021X_EAPOL */
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: 802.1X not included");
	return -1;
#endif /* IEEE8021X_EAPOL */
}


static int wpa_supplicant_ctrl_iface_status(struct wpa_supplicant *wpa_s,
					    const char *params,
					    char *buf, size_t buflen)
{
	char *pos, *end, tmp[30];
	int res, verbose, ret;

	verbose = os_strcmp(params, "-VERBOSE") == 0;
	pos = buf;
	end = buf + buflen;
	if (wpa_s->wpa_state >= WPA_ASSOCIATED) {
		struct wpa_ssid *ssid = wpa_s->current_ssid;
		ret = os_snprintf(pos, end - pos, "bssid=" MACSTR "\n",
				  MAC2STR(wpa_s->bssid));
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
		if (ssid) {
			u8 *_ssid = ssid->ssid;
			size_t ssid_len = ssid->ssid_len;
			u8 ssid_buf[MAX_SSID_LEN];
			if (ssid_len == 0) {
				int _res = wpa_drv_get_ssid(wpa_s, ssid_buf);
				if (_res < 0)
					ssid_len = 0;
				else
					ssid_len = _res;
				_ssid = ssid_buf;
			}
			ret = os_snprintf(pos, end - pos, "ssid=%s\nid=%d\n",
					  wpa_ssid_txt(_ssid, ssid_len),
					  ssid->id);
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;

			if (ssid->id_str) {
				ret = os_snprintf(pos, end - pos,
						  "id_str=%s\n",
						  ssid->id_str);
				if (ret < 0 || ret >= end - pos)
					return pos - buf;
				pos += ret;
			}
		}

		pos += wpa_sm_get_status(wpa_s->wpa, pos, end - pos, verbose);
	}
	ret = os_snprintf(pos, end - pos, "wpa_state=%s\n",
			  wpa_supplicant_state_txt(wpa_s->wpa_state));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	if (wpa_s->l2 &&
	    l2_packet_get_ip_addr(wpa_s->l2, tmp, sizeof(tmp)) >= 0) {
		ret = os_snprintf(pos, end - pos, "ip_address=%s\n", tmp);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

#ifdef MODIFIED_BY_SONY
	ret = os_snprintf(pos, end - pos, "mac_address=%02X:%02X:%02X:%02X:%02X:%02X\n",
				   wpa_s->own_addr[0], wpa_s->own_addr[1], wpa_s->own_addr[2],
				   wpa_s->own_addr[3], wpa_s->own_addr[4], wpa_s->own_addr[5]);
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;
#endif	/* MODIFIED_BY_SONY */

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		res = eapol_sm_get_status(wpa_s->eapol, pos, end - pos,
					  verbose);
		if (res >= 0)
			pos += res;
	}

	res = rsn_preauth_get_status(wpa_s->wpa, pos, end - pos, verbose);
	if (res >= 0)
		pos += res;

	return pos - buf;
}


static int wpa_supplicant_ctrl_iface_bssid(struct wpa_supplicant *wpa_s,
					   char *cmd)
{
	char *pos;
	int id;
	struct wpa_ssid *ssid;
	u8 bssid[ETH_ALEN];

	/* cmd: "<network id> <BSSID>" */
	pos = os_strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: id=%d bssid='%s'", id, pos);
	if (hwaddr_aton(pos, bssid)) {
		wpa_printf(MSG_DEBUG ,"CTRL_IFACE: invalid BSSID '%s'", pos);
		return -1;
	}

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find SSID id=%d "
			   "to update", id);
		return -1;
	}

	os_memcpy(ssid->bssid, bssid, ETH_ALEN);
	ssid->bssid_set =
		os_memcmp(bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) != 0;
		

	return 0;
}


static int wpa_supplicant_ctrl_iface_list_networks(
	struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	char *pos, *end;
	struct wpa_ssid *ssid;
	int ret;

	pos = buf;
	end = buf + buflen;
	ret = os_snprintf(pos, end - pos,
			  "network id / ssid / bssid / flags\n");
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	ssid = wpa_s->conf->ssid;
	while (ssid) {
		ret = os_snprintf(pos, end - pos, "%d\t%s",
				  ssid->id,
				  wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
		if (ssid->bssid_set) {
			ret = os_snprintf(pos, end - pos, "\t" MACSTR,
					  MAC2STR(ssid->bssid));
		} else {
			ret = os_snprintf(pos, end - pos, "\tany");
		}
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
		ret = os_snprintf(pos, end - pos, "\t%s%s",
				  ssid == wpa_s->current_ssid ?
				  "[CURRENT]" : "",
				  ssid->disabled ? "[DISABLED]" : "");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		ssid = ssid->next;
	}

	return pos - buf;
}


static char * wpa_supplicant_cipher_txt(char *pos, char *end, int cipher)
{
	int first = 1, ret;
	ret = os_snprintf(pos, end - pos, "-");
	if (ret < 0 || ret >= end - pos)
		return pos;
	pos += ret;
	if (cipher & WPA_CIPHER_NONE) {
		ret = os_snprintf(pos, end - pos, "%sNONE", first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}
	if (cipher & WPA_CIPHER_WEP40) {
		ret = os_snprintf(pos, end - pos, "%sWEP40", first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}
	if (cipher & WPA_CIPHER_WEP104) {
		ret = os_snprintf(pos, end - pos, "%sWEP104",
				  first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}
	if (cipher & WPA_CIPHER_TKIP) {
		ret = os_snprintf(pos, end - pos, "%sTKIP", first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}
	if (cipher & WPA_CIPHER_CCMP) {
		ret = os_snprintf(pos, end - pos, "%sCCMP", first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}
	return pos;
}


static char * wpa_supplicant_ie_txt(char *pos, char *end, const char *proto,
				    const u8 *ie, size_t ie_len)
{
	struct wpa_ie_data data;
	int first, ret;

	ret = os_snprintf(pos, end - pos, "[%s-", proto);
	if (ret < 0 || ret >= end - pos)
		return pos;
	pos += ret;

	if (wpa_parse_wpa_ie(ie, ie_len, &data) < 0) {
#ifdef EAP_WPS
		struct wps_data *wps = 0;
		if (wps_create_wps_data(&wps) < 0 ||
		    wps_parse_wps_ie(ie, ie_len, wps) < 0) {
			if (wps)
				wps_destroy_wps_data(&wps);
#endif /* EAP_WPS */
		ret = os_snprintf(pos, end - pos, "?]");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		return pos;
#ifdef EAP_WPS
		} else {
			u8 scState;
			Boolean selReg;
			u16 devPwdId;
			first = 1;
			do {
				if (!wps_get_value(wps, WPS_TYPE_WPSSTATE, &scState, NULL)) {
					ret = os_snprintf(pos, end - pos, "%s%s",
								   first ? "" : "+",
								   (WPS_WPSSTATE_UNCONFIGURED == scState) ?
								   "Unconf" : "Conf");
					if (ret < 0 || ret >= end - pos)
						break;
					pos += ret;
					first = 0;
				}
				if (!wps_get_value(wps, WPS_TYPE_SEL_REGISTRAR, &selReg, NULL)) {
					if (selReg) {
						ret = os_snprintf(pos, end - pos, "%sSR",
									   first ? "" : "+");
						if (ret < 0 || ret >= end - pos)
							break;
						pos += ret;
						first = 0;
					}
					if (selReg &&
						!wps_get_value(wps, WPS_TYPE_DEVICE_PWD_ID, &devPwdId, NULL)) {
						if (WPS_DEVICEPWDID_PUSH_BTN == devPwdId) {
							ret = os_snprintf(pos, end - pos, "%sPBC",
										   first ? "" : "+");
							if (ret < 0 || ret >= end - pos)
								break;
							pos += ret;
							first = 0;
						}
					}
				}
				ret = os_snprintf(pos, end - pos, "]");
				pos += ret;
			} while (0);
			wps_destroy_wps_data(&wps);
			return pos;
		}
#endif /* EAP_WPS */
	}

	first = 1;
	if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		ret = os_snprintf(pos, end - pos, "%sEAP", first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}
	if (data.key_mgmt & WPA_KEY_MGMT_PSK) {
		ret = os_snprintf(pos, end - pos, "%sPSK", first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}
	if (data.key_mgmt & WPA_KEY_MGMT_WPA_NONE) {
		ret = os_snprintf(pos, end - pos, "%sNone", first ? "" : "+");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
		first = 0;
	}

	pos = wpa_supplicant_cipher_txt(pos, end, data.pairwise_cipher);

	if (data.capabilities & WPA_CAPABILITY_PREAUTH) {
		ret = os_snprintf(pos, end - pos, "-preauth");
		if (ret < 0 || ret >= end - pos)
			return pos;
		pos += ret;
	}

	ret = os_snprintf(pos, end - pos, "]");
	if (ret < 0 || ret >= end - pos)
		return pos;
	pos += ret;

	return pos;
}


static int wpa_supplicant_ctrl_iface_scan_results(
	struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	char *pos, *end;
	struct wpa_scan_result *res;
	int i, ret;

	if (wpa_s->scan_results == NULL &&
	    wpa_supplicant_get_scan_results(wpa_s) < 0)
		return 0;
	if (wpa_s->scan_results == NULL)
		return 0;

	pos = buf;
	end = buf + buflen;
	ret = os_snprintf(pos, end - pos, "bssid / frequency / signal level / "
			  "flags / ssid\n");
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	for (i = 0; i < wpa_s->num_scan_results; i++) {
		res = &wpa_s->scan_results[i];
		ret = os_snprintf(pos, end - pos, MACSTR "\t%d\t%d\t",
				  MAC2STR(res->bssid), res->freq, res->level);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
		if (res->wpa_ie_len) {
			pos = wpa_supplicant_ie_txt(pos, end, "WPA",
						    res->wpa_ie,
						    res->wpa_ie_len);
		}
		if (res->rsn_ie_len) {
			pos = wpa_supplicant_ie_txt(pos, end, "WPA2",
						    res->rsn_ie,
						    res->rsn_ie_len);
		}
#ifdef EAP_WPS
		if (res->wps_ie_len) {
			pos = wpa_supplicant_ie_txt(pos, end, "WPS",
						    res->wps_ie,
						    res->wps_ie_len);
		}
#endif /* EAP_WPS */
		if (!res->wpa_ie_len && !res->rsn_ie_len &&
		    res->caps & IEEE80211_CAP_PRIVACY) {
			ret = os_snprintf(pos, end - pos, "[WEP]");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (res->caps & IEEE80211_CAP_IBSS) {
			ret = os_snprintf(pos, end - pos, "[IBSS]");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}

		ret = os_snprintf(pos, end - pos, "\t%s",
				  wpa_ssid_txt(res->ssid, res->ssid_len));
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}


static int wpa_supplicant_ctrl_iface_select_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	/* cmd: "<network id>" or "any" */
	if (os_strcmp(cmd, "any") == 0) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SELECT_NETWORK any");
		ssid = wpa_s->conf->ssid;
		while (ssid) {
			ssid->disabled = 0;
			ssid = ssid->next;
		}
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
		return 0;
	}

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: SELECT_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (ssid != wpa_s->current_ssid && wpa_s->current_ssid)
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);

#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
	if (wpa_s->conf->wps->wps_job_busy &&
		(id != wpa_s->conf->wps->nwid_trying_wps)) {
		if (eap_wps_disable(wpa_s, wpa_s->conf->wps)) {
			wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not cancel "
				   "EAP-WPS methods");
			return -1;
		}
	}
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

	/* Mark all other networks disabled and trigger reassociation */
	ssid = wpa_s->conf->ssid;
	while (ssid) {
		ssid->disabled = id != ssid->id;
		ssid = ssid->next;
	}
	wpa_s->reassociate = 1;
	wpa_supplicant_req_scan(wpa_s, 0, 0);

	return 0;
}


static int wpa_supplicant_ctrl_iface_enable_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	/* cmd: "<network id>" */
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: ENABLE_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (wpa_s->current_ssid == NULL && ssid->disabled) {
		/*
		 * Try to reassociate since there is no current configuration
		 * and a new network was made available. */
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
	ssid->disabled = 0;

	return 0;
}


static int wpa_supplicant_ctrl_iface_disable_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	/* cmd: "<network id>" */
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: DISABLE_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (ssid == wpa_s->current_ssid)
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	ssid->disabled = 1;

	return 0;
}


static int wpa_supplicant_ctrl_iface_add_network(
	struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	struct wpa_ssid *ssid;
	int ret;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: ADD_NETWORK");

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL)
		return -1;
	ssid->disabled = 1;
	wpa_config_set_network_defaults(ssid);

	ret = os_snprintf(buf, buflen, "%d\n", ssid->id);
	if (ret < 0 || (size_t) ret >= buflen)
		return -1;
	return ret;
}


static int wpa_supplicant_ctrl_iface_remove_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	if (!os_strcmp(cmd, "all")) {
		while (wpa_s->conf->ssid) {
			wpa_config_remove_network(
				wpa_s->conf, wpa_s->conf->ssid->id);
		}
		return 0;
	}

	/* cmd: "<network id>" */
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: REMOVE_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL ||
	    wpa_config_remove_network(wpa_s->conf, id) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (ssid == wpa_s->current_ssid) {
		/*
		 * Invalidate the EAP session cache if the current network is
		 * removed.
		 */
		eapol_sm_invalidate_cached_session(wpa_s->eapol);

		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	}

	return 0;
}


static int wpa_supplicant_ctrl_iface_set_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;
	char *name, *value;

	/* cmd: "<network id> <variable name> <value>" */
	name = os_strchr(cmd, ' ');
	if (name == NULL)
		return -1;
	*name++ = '\0';

	value = os_strchr(name, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: SET_NETWORK id=%d name='%s'",
		   id, name);
	wpa_hexdump_ascii_key(MSG_DEBUG, "CTRL_IFACE: value",
			      (u8 *) value, os_strlen(value));

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (wpa_config_set(ssid, name, value, 0) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Failed to set network "
			   "variable '%s'", name);
		return -1;
	}

	if (wpa_s->current_ssid == ssid) {
		/*
		 * Invalidate the EAP session cache if anything in the current
		 * configuration changes.
		 */
		eapol_sm_invalidate_cached_session(wpa_s->eapol);
	}

	if ((os_strcmp(name, "psk") == 0 &&
	     value[0] == '"' && ssid->ssid_len) ||
	    (os_strcmp(name, "ssid") == 0 && ssid->passphrase))
		wpa_config_update_psk(ssid);

	return 0;
}


static int wpa_supplicant_ctrl_iface_get_network(
	struct wpa_supplicant *wpa_s, char *cmd, char *buf, size_t buflen)
{
	int id;
	struct wpa_ssid *ssid;
	char *name, *value;

	/* cmd: "<network id> <variable name>" */
	name = os_strchr(cmd, ' ');
	if (name == NULL || buflen == 0)
		return -1;
	*name++ = '\0';

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: GET_NETWORK id=%d name='%s'",
		   id, name);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	value = wpa_config_get_no_key(ssid, name);
	if (value == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Failed to get network "
			   "variable '%s'", name);
		return -1;
	}

	os_snprintf(buf, buflen, "%s", value);
	buf[buflen - 1] = '\0';

	os_free(value);

	return os_strlen(buf);
}


static int wpa_supplicant_ctrl_iface_save_config(struct wpa_supplicant *wpa_s)
{
	int ret;

	if (!wpa_s->conf->update_config) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SAVE_CONFIG - Not allowed "
			   "to update configuration (update_config=0)");
		return -1;
	}

	ret = wpa_config_write(wpa_s->confname, wpa_s->conf);
	if (ret) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SAVE_CONFIG - Failed to "
			   "update configuration");
	} else {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SAVE_CONFIG - Configuration"
			   " updated");
	}

	return ret;
}


static int wpa_supplicant_ctrl_iface_get_capability(
	struct wpa_supplicant *wpa_s, const char *_field, char *buf,
	size_t buflen)
{
	struct wpa_driver_capa capa;
	int res, first = 1, ret;
	char *pos, *end, *strict;
	char field[30];

	/* Determine whether or not strict checking was requested */
	os_snprintf(field, sizeof(field), "%s", _field);
	field[sizeof(field) - 1] = '\0';
	strict = os_strchr(field, ' ');
	if (strict != NULL) {
		*strict++ = '\0';
		if (os_strcmp(strict, "strict") != 0)
			return -1;
	}

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: GET_CAPABILITY '%s' %s",
		field, strict ? strict : "");

	if (os_strcmp(field, "eap") == 0) {
		return eap_get_names(buf, buflen);
	}

	res = wpa_drv_get_capa(wpa_s, &capa);

	pos = buf;
	end = pos + buflen;

	if (os_strcmp(field, "pairwise") == 0) {
		if (res < 0) {
			if (strict)
				return 0;
			ret = os_snprintf(buf, buflen, "CCMP TKIP NONE");
			if (ret < 0 || (size_t) ret >= buflen)
				return -1;
			return ret;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			ret = os_snprintf(pos, end - pos, "%sCCMP",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			ret = os_snprintf(pos, end - pos, "%sTKIP",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE) {
			ret = os_snprintf(pos, end - pos, "%sNONE",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		return pos - buf;
	}

	if (os_strcmp(field, "group") == 0) {
		if (res < 0) {
			if (strict)
				return 0;
			ret = os_snprintf(buf, buflen,
					  "CCMP TKIP WEP104 WEP40");
			if (ret < 0 || (size_t) ret >= buflen)
				return -1;
			return ret;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			ret = os_snprintf(pos, end - pos, "%sCCMP",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			ret = os_snprintf(pos, end - pos, "%sTKIP",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP104) {
			ret = os_snprintf(pos, end - pos, "%sWEP104",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP40) {
			ret = os_snprintf(pos, end - pos, "%sWEP40",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		return pos - buf;
	}

	if (os_strcmp(field, "key_mgmt") == 0) {
		if (res < 0) {
			if (strict)
				return 0;
			ret = os_snprintf(buf, buflen, "WPA-PSK WPA-EAP "
					  "IEEE8021X WPA-NONE NONE");
			if (ret < 0 || (size_t) ret >= buflen)
				return -1;
			return ret;
		}

		ret = os_snprintf(pos, end - pos, "NONE IEEE8021X");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2)) {
			ret = os_snprintf(pos, end - pos, " WPA-EAP");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
			ret = os_snprintf(pos, end - pos, " WPA-PSK");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE) {
			ret = os_snprintf(pos, end - pos, " WPA-NONE");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}

		return pos - buf;
	}

	if (os_strcmp(field, "proto") == 0) {
		if (res < 0) {
			if (strict)
				return 0;
			ret = os_snprintf(buf, buflen, "RSN WPA");
			if (ret < 0 || (size_t) ret >= buflen)
				return -1;
			return ret;
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
			ret = os_snprintf(pos, end - pos, "%sRSN",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK)) {
			ret = os_snprintf(pos, end - pos, "%sWPA",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		return pos - buf;
	}

	if (os_strcmp(field, "auth_alg") == 0) {
		if (res < 0) {
			if (strict)
				return 0;
			ret = os_snprintf(buf, buflen, "OPEN SHARED LEAP");
			if (ret < 0 || (size_t) ret >= buflen)
				return -1;
			return ret;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_OPEN)) {
			ret = os_snprintf(pos, end - pos, "%sOPEN",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_SHARED)) {
			ret = os_snprintf(pos, end - pos, "%sSHARED",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_LEAP)) {
			ret = os_snprintf(pos, end - pos, "%sLEAP",
					  first ? "" : " ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
			first = 0;
		}

		return pos - buf;
	}

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: Unknown GET_CAPABILITY field '%s'",
		   field);

	return -1;
}


static int wpa_supplicant_ctrl_iface_ap_scan(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ap_scan = atoi(cmd);

	if (ap_scan < 0 || ap_scan > 2)
		return -1;
	wpa_s->conf->ap_scan = ap_scan;
	return 0;
}


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
#if 0   /* WAS */
static int wpa_supplicant_ctrl_iface_wps_set_regmode(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;
	int regmode = atoi(cmd);
	int prev_regmode;

	do {
		if (!wps)
			break;

		prev_regmode = wps->reg_mode;
		if (0 == regmode)
			wps->reg_mode = WPS_SUPPLICANT_REGMODE_NONE;
		else if (1 == regmode)
			wps->reg_mode = WPS_SUPPLICANT_REGMODE_CONFIGURE_AP;
		else if (2 == regmode)
			wps->reg_mode = WPS_SUPPLICANT_REGMODE_REGISTER_AP;
		else if (3 == regmode)
			wps->reg_mode = WPS_SUPPLICANT_REGMODE_REGISTER_STA;
		else
			break;
		ret = 0;
	} while (0);

	return ret;
}
#endif  /* WAS */


/* Set the wps configuration of the interface 
 * to a copy of parameters from the given "network" configuration.
 */
#if 0   /* WAS */
static int wpa_supplicant_ctrl_iface_wps_set_configuration(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;
	struct wpa_ssid *ssid;
	int index = atoi(cmd);
	u8 *config = 0;
	size_t config_len = 0;

	do {
		if (!wps)
			break;

		if (wps->config) {
			os_free(wps->config);
			wps->config = 0;
			wps->config_len = 0;
		}

		ssid = wpa_config_get_network(wpa_s->conf, index);
		if (!ssid)
			break;
		if (wps->reg_mode == WPS_SUPPLICANT_REGMODE_CONFIGURE_AP)
			ssid->disabled = 1;

		if (wps_get_supplicant_ssid_configuration(wpa_s, index, &config, &config_len))
			break;

		wps->config = config;
		wps->config_len = config_len;

		ret = 0;
	} while (0);

	if (ret && config)
		os_free(config);

	return ret;
}
#endif  /* WAS */


#if 0   /* WAS */
static int wpa_supplicant_ctrl_iface_wps_clear_password(
	struct wpa_supplicant *wpa_s)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;

	do {
		os_memset(wps->dev_pwd, 0, sizeof(wps->dev_pwd));
		wps->dev_pwd_len = 0;
		wps->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;

		if (wps->set_pub_key) {
			if (wps->dh_secret)
				eap_wps_free_dh(&wps->dh_secret);
			os_memset(wps->pub_key, 0, sizeof(wps->pub_key));
			wps->set_pub_key = 0;
		}
		ret = 0;
	} while (0);

	return ret;
}
#endif  /* WAS */
#endif /* USE_INTEL_SDK */


#ifdef WPS_OPT_UPNP
static  int wpa_supplicant_ctrl_iface_upnp_enabled(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	int enabled;
	struct wps_config *wps = wpa_s->conf->wps;

	do {
		if (!cmd)
			break;

		enabled = atoi(cmd);
		if (enabled) {
			if (wps_opt_upnp_sm_start(wpa_s->wps_opt_upnp, wps->upnp_iface))
				break;
		} else {
			if (wps_opt_upnp_sm_stop(wpa_s->wps_opt_upnp))
				break;
		}

		ret = 0;
	} while (0);

	return ret;
}
#endif  /* WPS_OPT_UPNP */


#ifdef WPS_OPT_UPNP
static  int wpa_supplicant_ctrl_iface_upnp_set_if(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;

	do {
		if (!cmd)
			break;

		if(wps->upnp_iface)
			os_free(wps->upnp_iface);
		wps->upnp_iface = os_strdup(cmd);
		if(!wps->upnp_iface)
			break;

		ret = 0;
	} while (0);

	return ret;
}
#endif  /* WPS_OPT_UPNP */


#ifdef WPS_OPT_UPNP
static int wpa_supplicant_ctrl_iface_wps_set_current_upnp_device(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;

	do {
		if (!cmd)
			break;

		if (wps->cur_upnp_device)
			os_free(wps->cur_upnp_device);

		wps->cur_upnp_device = os_strdup(cmd);
		if(!wps->cur_upnp_device)
			break;

		ret = 0;
	} while (0);

	return ret;
}
#endif  /* WPS_OPT_UPNP */


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
/* CONFIGSTOP   -- stop WPS configuration if any
 */
static int wpa_supplicant_ctrl_iface_configstop(
	struct wpa_supplicant *wpa_s)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;
        wpa_printf(MSG_INFO, "CONFIGSTOP -- stopping WPS operation if any");
        if (wps) ret = eap_wps_disable(wpa_s, wps);
        else wpa_printf(MSG_ERROR, "CONFIGSTOP -- NO wps struct found");
        return ret;
}
#endif
#endif


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
/* CONFIGME [<tag>=<value>]...
 * Tags are:
 * pin=<digits>         -- specify password
 * upnp=[{0|1}]         -- nonzero to use UPnP configuration
 * ssid=<text>          -- restrict search to given SSID
 * ssid="<text>["]
 * ssid=0x<hex>
 * bssid=<hex>:<hex>:<hex>:<hex>:<hex>:<hex>  -- restrict search to given BSSID
 * nosave[={0|1}]       -- nonzero to do NOT save result back to config file
 * clean                -- remove all old network configurations
 *
 * If no PIN given, push button mode is assumed.
 * If upnp mode is not given, it is zero (upnp not used),
 * but if upnp is given with out a value then it is one.
 * If nosave is not given, configuration is saved back to file,
 * but if it is given without a value then it is one (NOT saved).
 * ssid and/or bssid can be give as a filter,
 * after which there must be one PB-ready AP for PB mode;
 * for PIN mode there can be either one PIN-ready AP or else one
 * WPS-capable AP or else one open AP.
 */
static int wpa_supplicant_ctrl_iface_configme(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;
	char *password = "";
	size_t pwd_len = 0;
        int upnp_enable = 0;
        int filter_bssid_flag = 0;
        u8  filter_bssid[6];
        int filter_ssid_length = 0;
        u8  *filter_ssid = NULL;
        int nosave = 0;
        char *tag;
        char *value;

        for (;;) {
                /* Parse args */
                value = "";
                while (*cmd && !isgraph(*cmd)) cmd++;
                if (! *cmd) break;
                tag = cmd;
                while (isgraph(*cmd) && *cmd != '=') cmd++;
                if (*cmd == '=') {
                        *cmd++ = 0;
                        value = cmd;
                        while (isgraph(*cmd)) cmd++;
                } 
                if (*cmd) *cmd++ = 0;
                if (!strcmp(tag, "nosave")) {
                        if (*value) nosave = atol(value);
                        else nosave = 1;
                }
                if (!strcmp(tag, "pin")) {
                        password = value;
                        pwd_len = os_strlen(password);
                        continue;
                }
                if (!strcmp(tag, "clean")) {
                        wpa_printf(MSG_INFO, "Removing old network configs");
		        while (wpa_s->conf->ssid) {
			        wpa_config_remove_network(
				        wpa_s->conf, wpa_s->conf->ssid->id);
		        }
                        continue;
                }
                if (!strcmp(tag, "upnp")) {
                        if (*value) upnp_enable = atol(value);
                        else upnp_enable = 1;
                        continue;
                }
                if (!strcmp(tag, "ssid")) {
                        if (*value == 0) {
                                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGME: missing ssid "
	                                "'%s'", value);
                                return -1;
                        } 
                        filter_ssid = (void *)value;
                        filter_ssid_length = os_strlen(value);
                        if (*filter_ssid == '"') {
                                filter_ssid++;
                                filter_ssid_length--;
                                if (filter_ssid_length > 0 &&
                                        filter_ssid[filter_ssid_length-1] == '"') {
                                    filter_ssid[--filter_ssid_length] = 0;
                                }
                        } else
                        if (filter_ssid[0] == '0' && filter_ssid[1] == 'x') {
                                filter_ssid += 2;
                                filter_ssid_length -= 2;
                                filter_ssid_length /= 2;
                                if (hexstr2bin((void *)filter_ssid,
                                        filter_ssid, filter_ssid_length)) {
	                                wpa_printf(MSG_ERROR, 
                                                "CTRL_IFACE CONFIGME: invalid ssid "
		                                "'%s'", value);
	                                return -1;
                                }
                        }
                        continue;
                }
                if (!strcmp(tag, "bssid")) {
	                if (hwaddr_aton(value, filter_bssid)) {
		                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGME: invalid bssid "
			                "'%s'", value);
		                return -1;
	                }
                        filter_bssid_flag = 1;
                        continue;
                }
                wpa_printf(MSG_ERROR, "Unknown tag for CONFIGME: %s", tag);
                return -1;
        }

	if (wps) do {
                struct eap_wps_enable_params params = {};
                params.dev_pwd = (u8 *)password;
                params.dev_pwd_len = pwd_len,
                params.filter_bssid_flag = filter_bssid_flag;
                params.filter_bssid = filter_bssid;
                params.filter_ssid_length = filter_ssid_length;
                params.filter_ssid = filter_ssid;
                params.do_save = !nosave;
                wps->do_save = !nosave; /* %% FIX %% */
      		wps->reg_mode = WPS_SUPPLICANT_REGMODE_NONE; /* %% FIX %% */
                if (eap_wps_enable(wpa_s, wps, &params)) {
                        break;
                }
#ifdef WPS_OPT_UPNP
		if (upnp_enable) {
			if (wps_opt_upnp_sm_start(wpa_s->wps_opt_upnp, wps->upnp_iface))
				break;
		} else {
			if (wps_opt_upnp_sm_stop(wpa_s->wps_opt_upnp))
				break;
		}
#endif  /* WPS_OPT_UPNP */

		ret = 0;
	} while(0);

        if (ret) {
                wpa_printf(MSG_ERROR, "WPS failed from CONFIGME");
        }

	return ret;
}
#endif
#endif


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
/* CONFIGTHEM [<tag>=<value>]...
 * Tags are:
 * nwid=<number>        -- which "network" configuration to copy to AP
 * pin=<digits>         -- specify password
 * upnp=[{0|1}]         -- nonzero to use UPnP configuration
 * ssid=<text>          -- restrict search to given SSID
 * ssid="<text>["]
 * ssid=0x<hex>
 * bssid=<hex>:<hex>:<hex>:<hex>:<hex>:<hex>  -- restrict search to given BSSID
 *
 * nwid defaults to current network configuration if any, else error.
 * If no PIN given, push button mode is assumed.
 * If upnp mode is not given, it is zero (upnp not used),
 * but if upnp is given with out a value then it is one.
 * ssid and/or bssid can be give as a filter,
 * after which there must be one PB-ready AP for PB mode;
 * for PIN mode there can be either one PIN-ready AP or else one
 * WPS-capable AP or else one open AP.
 */
static int wpa_supplicant_ctrl_iface_configthem(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;
	struct wpa_ssid *ssid;
	u8 *config = 0;
	size_t config_len = 0;
        int nwid = -1;
	char *password = "";
	size_t pwd_len = 0;
        int upnp_enable = 0;
        int filter_bssid_flag = 0;
        u8  filter_bssid[6];
        int filter_ssid_length = 0;
        u8  *filter_ssid = NULL;
        char *tag;
        char *value;

        for (;;) {
                /* Parse args */
                value = "";
                while (*cmd && !isgraph(*cmd)) cmd++;
                if (! *cmd) break;
                tag = cmd;
                while (isgraph(*cmd) && *cmd != '=') cmd++;
                if (*cmd == '=') {
                        *cmd++ = 0;
                        value = cmd;
                        while (isgraph(*cmd)) cmd++;
                } 
                if (*cmd) *cmd++ = 0;
                if (!strcmp(tag, "nwid")) {
                        if (*value) nwid = atol(value);
                        continue;
                }
                if (!strcmp(tag, "pin")) {
                        password = value;
                        pwd_len = os_strlen(password);
                        continue;
                }
                if (!strcmp(tag, "upnp")) {
                        if (*value) upnp_enable = atol(value);
                        else upnp_enable = 1;
                        continue;
                }
                if (!strcmp(tag, "ssid")) {
                        if (*value == 0) {
                                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGTHEM: missing ssid "
	                                "'%s'", value);
                                return -1;
                        } 
                        filter_ssid = (void *)value;
                        filter_ssid_length = os_strlen(value);
                        if (*filter_ssid == '"') {
                                filter_ssid++;
                                filter_ssid_length--;
                                if (filter_ssid_length > 0 &&
                                        filter_ssid[filter_ssid_length-1] == '"') {
                                    filter_ssid[--filter_ssid_length] = 0;
                                }
                        } else
                        if (filter_ssid[0] == '0' && filter_ssid[1] == 'x') {
                                filter_ssid += 2;
                                filter_ssid_length -= 2;
                                filter_ssid_length /= 2;
                                if (hexstr2bin((void *)filter_ssid,
                                        filter_ssid, filter_ssid_length)) {
	                                wpa_printf(MSG_ERROR, 
                                                "CTRL_IFACE CONFIGTHEM: invalid ssid "
		                                "'%s'", value);
	                                return -1;
                                }
                        }
                        continue;
                }
                if (!strcmp(tag, "bssid")) {
	                if (hwaddr_aton(value, filter_bssid)) {
		                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGTHEM: invalid bssid "
			                "'%s'", value);
		                return -1;
	                }
                        filter_bssid_flag = 1;
                        continue;
                }
                wpa_printf(MSG_ERROR, "Unknown tag for CONFIGTHEM: %s", tag);
                return -1;
        }

	if (wps) do {
                struct eap_wps_enable_params params = {};

                if (nwid < 0) {
                        if (wpa_s->current_ssid) {
                                nwid = wpa_s->current_ssid->id;
                        }
                }
                if (nwid < 0) {
                        wpa_printf(MSG_ERROR, "CONFIGTHEM: need nwid!");
                        break;
                }
      		wps->reg_mode = WPS_SUPPLICANT_REGMODE_CONFIGURE_AP;
                wps->do_save = 0;

		if (wps->config) {
			os_free(wps->config);
			wps->config = 0;
			wps->config_len = 0;
		}

		ssid = wpa_config_get_network(wpa_s->conf, nwid);
		if (!ssid)
			break;
                #if 0   /* Sony did this for WPS_SET_CONFIGURATION -- why?? %%%%%%%%% */
      		ssid->disabled = 1;
                #endif

		if (wps_get_supplicant_ssid_configuration(wpa_s, ssid->id, &config, &config_len))
			break;

		wps->config = config;
		wps->config_len = config_len;

                params.dev_pwd = (u8 *)password;
                params.dev_pwd_len = pwd_len,
                params.filter_bssid_flag = filter_bssid_flag;
                params.filter_bssid = filter_bssid;
                params.filter_ssid_length = filter_ssid_length;
                params.filter_ssid = filter_ssid;
                if (eap_wps_enable(wpa_s, wps, &params)) {
                        break;
                }
#ifdef WPS_OPT_UPNP
		if (upnp_enable) {
			if (wps_opt_upnp_sm_start(wpa_s->wps_opt_upnp, wps->upnp_iface))
				break;
		} else {
			if (wps_opt_upnp_sm_stop(wpa_s->wps_opt_upnp))
				break;
		}
#endif  /* WPS_OPT_UPNP */

		ret = 0;
	} while(0);

        if (ret) {
                wpa_printf(MSG_ERROR, "WPS failed from CONFIGTHEM");
        }

	return ret;
}
#endif
#endif


#if 0   /* WAS */
static int wpa_supplicant_ctrl_iface_wps_set_password(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = wpa_s->conf->wps;
	char *password;
	size_t pwd_len;

	do {
		if (!wps)
			break;
	        password = strchr(cmd, '\n');
	        if (password) *password = 0;
	        password = cmd;
	        pwd_len = os_strlen(password);
                if (pwd_len == 0) {
                        /* missing password means to disable */
                        eap_wps_disable(wpa_s, wps);
                        break;
                }
                if (eap_wps_enable(wpa_s, wps, 
                        (u8 *)password, pwd_len,
                        %%%%%%%%%%
                        )

                        %%%%%%%% hmm maybe filters should be set
                        %%%%%%%% by previous command
                        %%%%%% or else all by same command


%%%%%%%%%%%%%%%%%%%%  fix to call eap_wps_enable
int eap_wps_enable(struct wpa_supplicant *wpa_s, struct wps_config *wps,
                u8 *dev_pwd,    /* 00000000 for push button method */
                int dev_pwd_len,
                int filter_bssid_flag,   /* accept only given bssid? */
                u8  filter_bssid[6],     /* used if filter_bssid_flag */
                int filter_ssid_length,  /* accept only given essid? */
                u8  *filter_ssid)
%%%%%%%%%%%%%%%%%%

		os_memset(wps->dev_pwd, 0, sizeof(wps->dev_pwd));
		wps->dev_pwd_len = pwd_len;
		os_memcpy(wps->dev_pwd, password, wps->dev_pwd_len);

		if (pwd_len) {
			if ((8 == pwd_len) &&
				!eap_wps_device_password_validation((u8 *)password, pwd_len))
				wps->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
			else
				wps->dev_pwd_id = WPS_DEVICEPWDID_USER_SPEC;

			if (wps->set_pub_key) {
				if (wps->dh_secret)
					eap_wps_free_dh(&wps->dh_secret);
				os_memset(wps->pub_key, 0, sizeof(wps->pub_key));
				wps->set_pub_key = 0;
			}

			if (wps->reg_mode == WPS_SUPPLICANT_REGMODE_REGISTER_STA) {
				(void)wps_opt_upnp_set_selected_registrar(wpa_s->wps_opt_upnp,
														wps->cur_upnp_device, 1);
			} else if (wps->cur_upnp_device) {
				os_free(wps->cur_upnp_device);
				wps->cur_upnp_device = 0;;
			}
		} else {
			wps->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;

			if (wps->set_pub_key) {
				if (wps->dh_secret)
					eap_wps_free_dh(&wps->dh_secret);
				os_memset(wps->pub_key, 0, sizeof(wps->pub_key));
				wps->set_pub_key = 0;
			}
		}

		ret = 0;
	} while(0);

	return ret;
}
#endif  /* WAS */


#ifdef WPS_OPT_UPNP
static int wpa_supplicant_ctrl_iface_reflesh_device(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int timeout;

	timeout = atoi(cmd);

	if (wps_opt_upnp_refresh_device(wpa_s->wps_opt_upnp, timeout))
		return -1;
	else
		return 0;
}
#endif  /* WPS_OPT_UPNP */


#ifdef WPS_OPT_UPNP
static int wpa_supplicant_ctrl_iface_get_upnp_scan_results(
	struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	int ret = 0;
	int res;
	char *pos, *end;
	struct upnp_wps_ctrlpt_device_list *list = 0, *cur;
	struct upnp_wps_ctrlpt_device *device;

	do {
		if (wps_opt_upnp_get_scan_result(wpa_s->wps_opt_upnp,
										 &list))
			break;

		pos = buf;
		end = buf + buflen;
		res = os_snprintf(pos, end - pos, "control_url / udn / "
					   "manufacturer / model name / model number / "
					   "serial_number\n");
		if (ret < 0 || res >= end - pos)
			break;
		pos += res;

		cur = list;
		while (cur) {
			device = &cur->device;

			res = os_snprintf(pos, end - pos, "%s\t%s\t%s\t%s\t%s\t%s\n",
						   device->control_url, device->udn,
						   device->manufacturer, device->model_name,
						   device->model_number, device->serial_number);
			if (res < 0 || res >= end - pos)
				break;
			pos += res;

			cur = cur->next;
		}
		ret = pos - buf;
	} while (0);

	(void)upnp_wps_ctrlpt_destroy_device_list(list);

	return ret;
}
#endif  /* WPS_OPT_UPNP */


#ifdef WPS_OPT_UPNP
static int wpa_supplicant_ctrl_iface_send_upnp_get_device_info(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;

	do {
		if (wps_opt_upnp_get_device_info(wpa_s->wps_opt_upnp, cmd))
			break;

		ret = 0;
	} while (0);

	return ret;
}
#endif  /* WPS_OPT_UPNP */


#ifdef WPS_OPT_UPNP
static int wpa_supplicant_ctrl_iface_set_upnp_selected_registrar(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int ret = -1;
	char ctrl_url[NAME_SIZE];
	int selected;

	do {
		if (2 != sscanf(cmd, "%s %d", ctrl_url, &selected))
			break;

		if (wps_opt_upnp_set_selected_registrar(wpa_s->wps_opt_upnp,
												ctrl_url, selected))
			break;

		ret = 0;
	} while (0);

	return ret;
}
#endif /* WPS_OPT_UPNP */


#ifdef WPS_OPT_NFC
static int wpa_supplicant_ctrl_iface_cancel_nfc_command(
	struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: CANCEL_NFC_COMMAND");

	if (wps_opt_nfc_cancel_nfc_comand(wpa_s->wps_opt_nfc))
		return -1;
	return 0;
}


static int wpa_supplicant_ctrl_iface_read_password_token(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: READ_PASSWORD_TOKEN id=%d", id);

	if (wps_opt_nfc_read_password_token(wpa_s->wps_opt_nfc, id))
		return -1;
	return 0;
}


static int wpa_supplicant_ctrl_iface_write_password_token(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: WRITE_PASSWORD_TOKEN id=%d", id);

	if (wps_opt_nfc_write_password_token(wpa_s->wps_opt_nfc, id))
		return -1;
	return 0;
}


static int wpa_supplicant_ctrl_iface_read_config_token(
	struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: READ_CONFIG_TOKEN");

	if (wps_opt_nfc_read_config_token(wpa_s->wps_opt_nfc))
		return -1;
	return 0;
}


static int wpa_supplicant_ctrl_iface_write_config_token(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: WRITE_CONFIG_TOKEN id=%d", id);

	if (wps_opt_nfc_write_config_token(wpa_s->wps_opt_nfc, id))
		return -1;
	return 0;
}
#endif /* WPS_OPT_NFC */
#endif /* EAP_WPS */


char * wpa_supplicant_ctrl_iface_process(struct wpa_supplicant *wpa_s,
					 char *buf, size_t *resp_len)
{
	char *reply;
	const int reply_size = 2048;
	int ctrl_rsp = 0;
	int reply_len;

	if (os_strncmp(buf, WPA_CTRL_RSP, os_strlen(WPA_CTRL_RSP)) == 0 ||
	    os_strncmp(buf, "SET_NETWORK ", 12) == 0) {
		wpa_hexdump_ascii_key(MSG_DEBUG, "RX ctrl_iface",
				      (const u8 *) buf, os_strlen(buf));
#ifdef MODIFIED_BY_SONY
	} else if ((0 == os_strncmp(buf, "PING", 4)) ||
			   (0 == os_strncmp(buf, "STATUS", 6))) {
		wpa_hexdump_ascii(MSG_MSGDUMP, "RX ctrl_iface",
				  (const u8 *) buf, os_strlen(buf));
#endif /* MODIFIED_BY_SONY */
	} else {
		wpa_hexdump_ascii(MSG_DEBUG, "RX ctrl_iface",
				  (const u8 *) buf, os_strlen(buf));
	}

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		*resp_len = 1;
		return NULL;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strcmp(buf, "MIB") == 0) {
		reply_len = wpa_sm_get_mib(wpa_s->wpa, reply, reply_size);
		if (reply_len >= 0) {
			int res;
			res = eapol_sm_get_mib(wpa_s->eapol, reply + reply_len,
					       reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
	} else if (os_strncmp(buf, "STATUS", 6) == 0) {
		reply_len = wpa_supplicant_ctrl_iface_status(
			wpa_s, buf + 6, reply, reply_size);
	} else if (os_strcmp(buf, "PMKSA") == 0) {
		reply_len = pmksa_cache_list(wpa_s->wpa, reply, reply_size);
	} else if (os_strncmp(buf, "SET ", 4) == 0) {
		if (wpa_supplicant_ctrl_iface_set(wpa_s, buf + 4))
			reply_len = -1;
	} else if (os_strcmp(buf, "LOGON") == 0) {
		eapol_sm_notify_logoff(wpa_s->eapol, FALSE);
	} else if (os_strcmp(buf, "LOGOFF") == 0) {
		eapol_sm_notify_logoff(wpa_s->eapol, TRUE);
	} else if (os_strcmp(buf, "REASSOCIATE") == 0) {
		wpa_s->disconnected = 0;
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	} else if (os_strcmp(buf, "RECONNECT") == 0) {
		if (wpa_s->disconnected) {
			wpa_s->disconnected = 0;
			wpa_s->reassociate = 1;
			wpa_supplicant_req_scan(wpa_s, 0, 0);
		}
	} else if (os_strncmp(buf, "PREAUTH ", 8) == 0) {
		if (wpa_supplicant_ctrl_iface_preauth(wpa_s, buf + 8))
			reply_len = -1;
#ifdef CONFIG_PEERKEY
	} else if (os_strncmp(buf, "STKSTART ", 9) == 0) {
		if (wpa_supplicant_ctrl_iface_stkstart(wpa_s, buf + 9))
			reply_len = -1;
#endif /* CONFIG_PEERKEY */
	} else if (os_strncmp(buf, WPA_CTRL_RSP, os_strlen(WPA_CTRL_RSP)) == 0)
	{
		if (wpa_supplicant_ctrl_iface_ctrl_rsp(
			    wpa_s, buf + os_strlen(WPA_CTRL_RSP)))
			reply_len = -1;
		else
			ctrl_rsp = 1;
	} else if (os_strcmp(buf, "RECONFIGURE") == 0) {
		if (wpa_supplicant_reload_configuration(wpa_s))
			reply_len = -1;
	} else if (os_strcmp(buf, "RECONFIGURE_ALL") == 0) {
		if (wpa_supplicant_reload_configuration_all(wpa_s->global))
			reply_len = -1;
	} else if (os_strcmp(buf, "TERMINATE") == 0) {
		eloop_terminate();
	} else if (os_strcmp(buf, "QUIT") == 0) {
		eloop_terminate();
	} else if (os_strncmp(buf, "BSSID ", 6) == 0) {
		if (wpa_supplicant_ctrl_iface_bssid(wpa_s, buf + 6))
			reply_len = -1;
	} else if (os_strcmp(buf, "LIST_NETWORKS") == 0) {
		reply_len = wpa_supplicant_ctrl_iface_list_networks(
			wpa_s, reply, reply_size);
	} else if (os_strcmp(buf, "DISCONNECT") == 0) {
		wpa_s->reassociate = 0;
		wpa_s->disconnected = 1;
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	} else if (os_strcmp(buf, "SCAN") == 0) {
		wpa_s->scan_req = 2;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	} else if (os_strcmp(buf, "SCAN_RESULTS") == 0) {
		reply_len = wpa_supplicant_ctrl_iface_scan_results(
			wpa_s, reply, reply_size);
	} else if (os_strncmp(buf, "SELECT_NETWORK ", 15) == 0) {
		if (wpa_supplicant_ctrl_iface_select_network(wpa_s, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "ENABLE_NETWORK ", 15) == 0) {
		if (wpa_supplicant_ctrl_iface_enable_network(wpa_s, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "DISABLE_NETWORK ", 16) == 0) {
		if (wpa_supplicant_ctrl_iface_disable_network(wpa_s, buf + 16))
			reply_len = -1;
	} else if (os_strcmp(buf, "ADD_NETWORK") == 0) {
		reply_len = wpa_supplicant_ctrl_iface_add_network(
			wpa_s, reply, reply_size);
	} else if (os_strncmp(buf, "REMOVE_NETWORK ", 15) == 0) {
		if (wpa_supplicant_ctrl_iface_remove_network(wpa_s, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_NETWORK ", 12) == 0) {
		if (wpa_supplicant_ctrl_iface_set_network(wpa_s, buf + 12))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_NETWORK ", 12) == 0) {
		reply_len = wpa_supplicant_ctrl_iface_get_network(
			wpa_s, buf + 12, reply, reply_size);
	} else if (os_strcmp(buf, "SAVE_CONFIG") == 0) {
		if (wpa_supplicant_ctrl_iface_save_config(wpa_s))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_CAPABILITY ", 15) == 0) {
		reply_len = wpa_supplicant_ctrl_iface_get_capability(
			wpa_s, buf + 15, reply, reply_size);
	} else if (os_strncmp(buf, "AP_SCAN ", 8) == 0) {
		if (wpa_supplicant_ctrl_iface_ap_scan(wpa_s, buf + 8))
			reply_len = -1;
	} else if (os_strcmp(buf, "INTERFACES") == 0) {
		reply_len = wpa_supplicant_global_iface_interfaces(
			wpa_s->global, reply, reply_size);
#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
	} else if (os_strncmp(buf, "CONFIGME", 8) == 0) {
		if (wpa_supplicant_ctrl_iface_configme(wpa_s, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "CONFIGTHEM", 10) == 0) {
		if (wpa_supplicant_ctrl_iface_configthem(wpa_s, buf + 10))
			reply_len = -1;
	} else if (os_strncmp(buf, "CONFIGSTOP", 10) == 0) {
		if (wpa_supplicant_ctrl_iface_configstop(wpa_s))
			reply_len = -1;
        #if 0   /* WAS */
	} else if (os_strncmp(buf, "WPS_SET_REGMODE ", 16) == 0) {
		if (wpa_supplicant_ctrl_iface_wps_set_regmode(wpa_s, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_SET_CONFIGURATION ", 22) == 0) {
		if (wpa_supplicant_ctrl_iface_wps_set_configuration(wpa_s, buf + 22))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_SET_PASSWORD ", 17) == 0) {
		if (wpa_supplicant_ctrl_iface_wps_set_password(wpa_s, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_CLEAR_PASSWORD", 18) == 0) {
		if (wpa_supplicant_ctrl_iface_wps_clear_password(wpa_s))
			reply_len = -1;
        #endif  /* WAS */
#endif /* USE_INTEL_SDK */
#ifdef WPS_OPT_UPNP
	} else if (os_strncmp(buf, "UPNP_ENABLED ", 13) == 0) {
		if (wpa_supplicant_ctrl_iface_upnp_enabled(wpa_s, buf + 13))
			reply_len = -1;
	} else if (os_strncmp(buf, "UPNP_SET_IF ", 12) == 0) {
		if (wpa_supplicant_ctrl_iface_upnp_set_if(wpa_s, buf + 12))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_SET_UPNP_DEVICE ", 20) == 0) {
		if (wpa_supplicant_ctrl_iface_wps_set_current_upnp_device(wpa_s, buf + 20))
			reply_len = -1;
	} else if (os_strncmp(buf, "UPNP_REFRESH ", 13) == 0) {
		if (wpa_supplicant_ctrl_iface_reflesh_device(wpa_s, buf + 13))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_UPNP_SCAN_RESULTS", 21) == 0) {
		reply_len = wpa_supplicant_ctrl_iface_get_upnp_scan_results(
			wpa_s, reply, reply_size);
	} else if (os_strncmp(buf, "SEND_UPNP_GETDEVINFO ", 21) == 0) {
		if (wpa_supplicant_ctrl_iface_send_upnp_get_device_info(wpa_s, buf + 21))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_UPNP_SEL_REG ", 17) == 0) {
		if (wpa_supplicant_ctrl_iface_set_upnp_selected_registrar(wpa_s, buf + 17))
			reply_len = -1;
#endif /* WPS_OPT_UPNP */
#ifdef WPS_OPT_NFC
	} else if (os_strncmp(buf, "CANCEL_NFC_COMMAND", 18) == 0) {
		if (wpa_supplicant_ctrl_iface_cancel_nfc_command(wpa_s))
			reply_len = -1;
	} else if (os_strncmp(buf, "READ_PASSWORD_TOKEN ", 20) == 0) {
		if (wpa_supplicant_ctrl_iface_read_password_token(wpa_s, buf + 20))
			reply_len = -1;
	} else if (os_strncmp(buf, "WRITE_PASSWORD_TOKEN ", 21) == 0) {
		if (wpa_supplicant_ctrl_iface_write_password_token(wpa_s, buf + 21))
			reply_len = -1;
	} else if (os_strncmp(buf, "READ_CONFIG_TOKEN", 17) == 0) {
		if (wpa_supplicant_ctrl_iface_read_config_token(wpa_s))
			reply_len = -1;
	} else if (os_strncmp(buf, "WRITE_CONFIG_TOKEN ", 19) == 0) {
		if (wpa_supplicant_ctrl_iface_write_config_token(wpa_s, buf + 19))
			reply_len = -1;
#endif /* WPS_OPT_NFC */
#endif /* EAP_WPS */
	} else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	if (ctrl_rsp)
		eapol_sm_notify_ctrl_response(wpa_s->eapol);

	*resp_len = reply_len;
	return reply;
}


static int wpa_supplicant_global_iface_add(struct wpa_global *global,
					   char *cmd)
{
	struct wpa_interface iface;
	char *pos;

	/*
	 * <ifname>TAB<confname>TAB<driver>TAB<ctrl_interface>TAB<driver_param>
	 * TAB<bridge_ifname>
	 */
	wpa_printf(MSG_DEBUG, "CTRL_IFACE GLOBAL INTERFACE_ADD '%s'", cmd);

	os_memset(&iface, 0, sizeof(iface));

	do {
		iface.ifname = pos = cmd;
		pos = os_strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.ifname[0] == '\0')
			return -1;
		if (pos == NULL)
			break;

		iface.confname = pos;
		pos = os_strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.confname[0] == '\0')
			iface.confname = NULL;
		if (pos == NULL)
			break;

		iface.driver = pos;
		pos = os_strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.driver[0] == '\0')
			iface.driver = NULL;
		if (pos == NULL)
			break;

		iface.ctrl_interface = pos;
		pos = os_strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.ctrl_interface[0] == '\0')
			iface.ctrl_interface = NULL;
		if (pos == NULL)
			break;

		iface.driver_param = pos;
		pos = os_strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.driver_param[0] == '\0')
			iface.driver_param = NULL;
		if (pos == NULL)
			break;

		iface.bridge_ifname = pos;
		pos = os_strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.bridge_ifname[0] == '\0')
			iface.bridge_ifname = NULL;
		if (pos == NULL)
			break;
	} while (0);

	if (wpa_supplicant_get_iface(global, iface.ifname))
		return -1;

	return wpa_supplicant_add_iface(global, &iface) ? 0 : -1;
}


static int wpa_supplicant_global_iface_remove(struct wpa_global *global,
					      char *cmd)
{
	struct wpa_supplicant *wpa_s;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GLOBAL INTERFACE_REMOVE '%s'", cmd);

	wpa_s = wpa_supplicant_get_iface(global, cmd);
	if (wpa_s == NULL)
		return -1;
	return wpa_supplicant_remove_iface(global, wpa_s);
}


static int wpa_supplicant_global_iface_interfaces(struct wpa_global *global,
						  char *buf, int len)
{
	int res;
	char *pos, *end;
	struct wpa_supplicant *wpa_s;

	wpa_s = global->ifaces;
	pos = buf;
	end = buf + len;

	while (wpa_s) {
		res = os_snprintf(pos, end - pos, "%s\n", wpa_s->ifname);
		if (res < 0 || res >= end - pos) {
			*pos = '\0';
			break;
		}
		pos += res;
		wpa_s = wpa_s->next;
	}
	return pos - buf;
}


char * wpa_supplicant_global_ctrl_iface_process(struct wpa_global *global,
						char *buf, size_t *resp_len)
{
	char *reply;
	const int reply_size = 2048;
	int reply_len;

	wpa_hexdump_ascii(MSG_DEBUG, "RX global ctrl_iface",
			  (const u8 *) buf, os_strlen(buf));

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		*resp_len = 1;
		return NULL;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "INTERFACE_ADD ", 14) == 0) {
		if (wpa_supplicant_global_iface_add(global, buf + 14))
			reply_len = -1;
	} else if (os_strncmp(buf, "INTERFACE_REMOVE ", 17) == 0) {
		if (wpa_supplicant_global_iface_remove(global, buf + 17))
			reply_len = -1;
	} else if (os_strcmp(buf, "INTERFACES") == 0) {
		reply_len = wpa_supplicant_global_iface_interfaces(
			global, reply, reply_size);
	} else if (os_strcmp(buf, "TERMINATE") == 0) {
		eloop_terminate();
	} else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	*resp_len = reply_len;
	return reply;
}
