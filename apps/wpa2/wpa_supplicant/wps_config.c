/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wps_config.c
//  Description: EAP-WPS config source
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

#include "defs.h"
#include "common.h"
#include "wpa_supplicant.h"
#include "wpa_supplicant_i.h"
#include "wpa.h"
#include "config.h"
#include "wps_config.h"
#include "wps_parser.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

extern int eap_wps_free_dh(void **dh);

int wps_config_free_dh(void **dh)
{
	return eap_wps_free_dh(dh);
}

extern int is_hex(const u8 *data, size_t len);

int wps_get_supplicant_ssid_configuration(void *ctx, int index, u8 **buf, size_t *len)
{
	int ret = -1;
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_ssid *ssid;
	struct wps_data *wps = 0;
	u16 auth, encr;
	Boolean enabled8021x = 0;
	u8 nwKeyIdx = -1;
	u8 nwKey[64 + 1];
	Boolean blval;
	size_t length;
	u8 nwIdx;

	do {
		if (!buf || !len)
			break;
		*buf = 0;
		*len = 0;

		ssid = wpa_config_get_network(wpa_s->conf, index);
		if (!ssid)
			break;

		if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
			if (ssid->proto & WPA_PROTO_RSN)
				auth = WPS_AUTHTYPE_WPA2;
			else
				auth = WPS_AUTHTYPE_WPA;

			if (ssid->pairwise_cipher & WPA_CIPHER_CCMP)
				encr = WPS_ENCRTYPE_AES;
			else
				encr = WPS_ENCRTYPE_TKIP;
			enabled8021x = 1;
		} else if (ssid->key_mgmt & WPA_KEY_MGMT_PSK) {
			if (ssid->proto & WPA_PROTO_RSN)
				auth = WPS_AUTHTYPE_WPA2PSK;
			else
				auth = WPS_AUTHTYPE_WPAPSK;

			if (ssid->pairwise_cipher & WPA_CIPHER_CCMP)
				encr = WPS_ENCRTYPE_AES;
			else
				encr = WPS_ENCRTYPE_TKIP;
			nwKeyIdx = 0;
		} else if (ssid->key_mgmt & WPA_KEY_MGMT_NONE) {
			if (ssid->auth_alg & WPA_AUTH_ALG_SHARED) {
				auth = WPS_AUTHTYPE_SHARED;
				nwKeyIdx = ssid->wep_tx_keyidx + 1;
			} else
				auth = WPS_AUTHTYPE_OPEN;

			if ((ssid->pairwise_cipher & (WPA_CIPHER_WEP40|WPA_CIPHER_WEP104)) ||
				(ssid->group_cipher & (WPA_CIPHER_WEP40|WPA_CIPHER_WEP104))) {
				encr = WPS_ENCRTYPE_WEP;
				nwKeyIdx = ssid->wep_tx_keyidx + 1;
			} else
				encr = WPS_ENCRTYPE_NONE;
		} else if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
			auth = WPS_AUTHTYPE_OPEN;
			encr = WPS_ENCRTYPE_WEP;
			enabled8021x = 1;
		}

		if (wps_create_wps_data(&wps))
			break;

		/* Network Index */
		nwIdx = ssid->id;
		if (wps_set_value(wps, WPS_TYPE_NW_INDEX, &nwIdx, 0))
			break;

		/* SSID */
		if (wps_set_value(wps, WPS_TYPE_SSID, ssid->ssid, ssid->ssid_len))
			break;

		/* Authentication Type */
		if (wps_set_value(wps, WPS_TYPE_AUTH_TYPE, &auth, 0))
			break;

		/* Encryption Type */
		if (wps_set_value(wps, WPS_TYPE_ENCR_TYPE, &encr, 0))
			break;

		if (nwKeyIdx != (u8)-1) {
			if (encr == WPS_ENCRTYPE_WEP) {
				if ((1 > nwKeyIdx) || (4 < nwKeyIdx)) {
					wpa_printf(MSG_WARNING, "Network Key Index is fixed. %d -> 1\n", nwKeyIdx);
					nwKeyIdx = 1;
				}

				/* Network Key Index (Option) */
				if (wps_set_value(wps, WPS_TYPE_NW_KEY_INDEX, &nwKeyIdx, 0))
					break;
			}

			/* Network Key */
			if (ssid->passphrase) {
				length = os_strlen(ssid->passphrase);
				strncpy((char *)nwKey, ssid->passphrase, length);
				nwKey[length] = 0;
			} else if (ssid->psk_set) {
				length = PMK_LEN * 2;
				wpa_snprintf_hex_uppercase((char *)nwKey, sizeof(nwKey), ssid->psk, length);
				nwKey[length] = 0;
			} else {
				if (is_hex(ssid->wep_key[nwKeyIdx - 1], ssid->wep_key_len[nwKeyIdx - 1])) {
					length = (u16)ssid->wep_key_len[nwKeyIdx - 1] * 2;
					wpa_snprintf_hex_uppercase((char *)nwKey, sizeof(nwKey), ssid->wep_key[nwKeyIdx - 1], length);
					nwKey[length] = 0;
				} else {
					length = (u16)ssid->wep_key_len[nwKeyIdx - 1];
					strncpy((char *)nwKey, (char *)ssid->wep_key[nwKeyIdx - 1], length);
					nwKey[length] = 0;
				}
			}
			if (wps_set_value(wps, WPS_TYPE_NW_KEY, nwKey, length))
				break;
		} else {
			/* Network Key (No Key) */
			if (wps_set_value(wps, WPS_TYPE_NW_KEY, 0, 0)) {
				break;
			}
		}

		/* MAC Address */
		if (wps_set_value(wps, WPS_TYPE_MAC_ADDR, wpa_s->own_addr, ETH_ALEN))
			break;

		if (enabled8021x) {
			char *value;

			/* EAP Type (Option) */
			value = wpa_config_get(ssid, "eap");
			if (value) {
				if (wps_set_value(wps, WPS_TYPE_EAP_TYPE, value, os_strlen(value))) {
					free(value);
					break;
				}
				free(value);
			}

			/* EAP Identity (Option) */
			value = wpa_config_get(ssid, "identity");
			if (value) {
				if (wps_set_value(wps, WPS_TYPE_EAP_IDENTITY, value, os_strlen(value))) {
					free(value);
					break;
				}
				free(value);
			}

			/* Key Provided Automaticaly (Option) */
			blval = 1;
			if (wps_set_value(wps, WPS_TYPE_KEY_PROVIDED_AUTO, &blval, 0))
				break;

			/* 802.1X Enabled (Option) */
			if (wps_set_value(wps, WPS_TYPE_8021X_ENABLED, &enabled8021x, 0))
				break;
		}

		if (wps_write_wps_data(wps, buf, len))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	if (ret) {
		if (buf && *buf) {
			free(*buf);
			*buf = 0;
		}
		if (len)
			*len = 0;
	}

	return ret;
}


/* Optionally share new configuration with other software,
* e.g. with hostapd for the repeater case.
* Does nothing unless newsettings_command is provided as part
* of the wps configuration.
*
* This is pretty fragile, there are a lot of things that can go wrong
* (starting with the "new settings command" not working properly).
* Requiring another program also means more flash space used.
* Better solution would be to combine wpa_supplicant and hostapd into
* one program, in which case the configuration could be shared by
* function call.
*/
static void wps_new_configuration_share(
	struct wpa_supplicant *wpa_s,
        u8 *buf, 
        size_t len)
{
        char *command_name;
        char *filepath = NULL;
        int fd;
        int pid;

        if (!wpa_s || !wpa_s->confname || !*wpa_s->confname ||
                        !wpa_s->conf || !wpa_s->conf->wps) 
                return;
        /* Do this only if configured with a command to execute
         * to do the sharing.
         */
        command_name = wpa_s->conf->wps->newsettings_command;
        if (!command_name || !*command_name)
                return;
        /* Overwrite the same file each time... this should be ok...
         * use the configuration file as the base for the file path,
         * since generally we have write permission for the configuration
         * file and thus probably for it's directory...
         */
        filepath = malloc(strlen(wpa_s->confname)+20);
        if (!filepath)
                return;
        sprintf(filepath, "%s-NEWWPS", wpa_s->confname);
        fd = open(filepath, O_WRONLY|O_TRUNC|O_CREAT, 0666);
        if (fd < 0) {
                wpa_printf(MSG_ERROR, "Failed to create %s", filepath);
                free(filepath);
                return;
        }
        if (write(fd, buf, len) != len) {
                wpa_printf(MSG_ERROR, "Failed to write %s", filepath);
                free(filepath);
                close(fd);
                return;
        }
        close(fd);
        wpa_printf(MSG_INFO, "Wrote WPS settings file %s", filepath);

        pid = fork();
        if (pid == -1) {
                wpa_printf(MSG_ERROR, "Fork failure");
        } else
        if (pid == 0) {
                /* child */
                /* fork again to prevent zombies */
                pid = fork();
                if (pid == -1) {
                        wpa_printf(MSG_ERROR, "Fork failure");
                } else
                if (pid == 0) {
                    /* child */
                    /* Invoke the given program or script,
                     * with filepath as argument.
                     */
                    wpa_printf(MSG_INFO, "Executing %s", command_name);
                    execlp(command_name, command_name, filepath, NULL);
                    wpa_printf(MSG_ERROR, "Failed to exec %s", command_name);
                    _exit(1);
                }
                _exit(0);
        } else {
                /* wait to prevent zombie.
                 * Note that this wait is short, since our immediate
                 * child forks again (see above) and exits.
                 */
                int status;
                waitpid(pid, &status, 0);
        }
        free(filepath);
        return;
}

/*
 * This installs received WPS configuration into a new network configuration.
 * It is optional that the new network configuration be saved to the
 * original configuration file.
 */
int wps_set_supplicant_ssid_configuration(void *ctx, u8 *buf, size_t len)
{
	int ret = -1;
	struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)ctx;
	struct wpa_ssid *ssid = 0;
	struct wps_data *wps = 0;
	u8 str_ssid[33];
	size_t ssid_length;
	u16 auth, encr;
	u8 nwKeyIdx;
	u8 *nwKey = 0;
	size_t nwKey_length;
	u8 macAddr[6];
	char *eapType = 0;
	char *eapIdentity = 0;
	Boolean keyProvideAuto;
	Boolean enabled8021X;
	Boolean passphrase = 0;
	size_t length;
	char *var, *value;

	do {
		if (!wpa_s)
			break;

                /* Optionally share new configuration with other software,
                 * e.g. with hostapd.
                 */
                wps_new_configuration_share(wpa_s, buf, len);

		ssid = wpa_config_add_network(wpa_s->conf);
		if (!ssid)
			break;
		wpa_config_set_network_defaults(ssid);

		if (wps_create_wps_data(&wps))
			break;

		if(wps_parse_wps_data(buf, len, wps))
			break;

		/* SSID */
		ssid_length = sizeof(str_ssid);
		if(wps_get_value(wps, WPS_TYPE_SSID, str_ssid, &ssid_length))
			break;
		str_ssid[ssid_length] = 0;

		/* Authentication Type */
		if (wps_get_value(wps, WPS_TYPE_AUTH_TYPE, &auth, NULL))
			break;

		/* Encryption Type */
		if (wps_get_value(wps, WPS_TYPE_ENCR_TYPE, &encr, NULL))
			break;

		/* Network Key Index (Option) */
		if(wps_get_value(wps, WPS_TYPE_NW_KEY_INDEX, &nwKeyIdx, NULL))
			nwKeyIdx = 1;
		if ((1 > nwKeyIdx) || (4 < nwKeyIdx)) { /* warning */
			wpa_printf(MSG_WARNING, "Network Key Index is fixed. %d -> 1\n", nwKeyIdx);
			nwKeyIdx = 1;
		}

		/* Network Key */
		nwKey_length = 0;
		(void)wps_get_value(wps, WPS_TYPE_NW_KEY, NULL, &nwKey_length);
		if (nwKey_length) {
			nwKey = (u8 *)calloc(1, nwKey_length + 1);
			if (!nwKey)
				break;
			if (wps_get_value(wps, WPS_TYPE_NW_KEY, nwKey, &nwKey_length)) {
				break;
			}
			nwKey[nwKey_length] = 0;
		}

		/* MAC Address */
		length = sizeof(macAddr);
		if(wps_get_value(wps, WPS_TYPE_MAC_ADDR, macAddr, &length))
			break;

		/* EAP Type (Option) */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_EAP_TYPE, NULL, &length);
		if (length) {
			eapType = (char *)calloc(1, length + 1);
			if (!eapType)
				break;
			if (wps_get_value(wps, WPS_TYPE_EAP_TYPE, eapType, &length)) {
				break;
			}
			eapType[length] = 0;
		}

		/* EAP Identity (Option) */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_EAP_IDENTITY, NULL, &length);
		if (length) {
			eapIdentity = (char *)calloc(1, length + 1);
			if (!eapIdentity)
				break;
			if (wps_get_value(wps, WPS_TYPE_EAP_IDENTITY, eapIdentity, &length)) {
				break;
			}
			eapIdentity[length] = 0;
		}

		/* Key Provided Automaticaly (Option) */
		if(wps_get_value(wps, WPS_TYPE_KEY_PROVIDED_AUTO, &keyProvideAuto, NULL))
			keyProvideAuto = 0;

		/* 802.1X Enabled (Option) */
		if(wps_get_value(wps, WPS_TYPE_8021X_ENABLED, &enabled8021X, NULL))
			enabled8021X = 0;

		/* Set Configuration */
                /* %%%%%% Sony set disabled to 1... why? */
		ssid->disabled = 0;
		/* ssid */
		var = "ssid";
		value = (char *)os_malloc(os_strlen((char *)str_ssid) + 3);
		if (!value)
			break;
		os_snprintf(value, os_strlen((char *)str_ssid) + 3, "\"%s\"", str_ssid);
		if (wpa_config_set(ssid, var, value, 0))
			break;
		free(value);

		/* auth_alg */
		var = "auth_alg";
		if (WPS_AUTHTYPE_SHARED == auth)
			value = "SHARED";
		else
			value = "OPEN";
		if (wpa_config_set(ssid, var, value, 0))
			break;

		/* key_mgmt */
		var = "key_mgmt";
		switch (auth) {
		case WPS_AUTHTYPE_OPEN:
		case WPS_AUTHTYPE_SHARED:
			if (enabled8021X)
				value = "IEEE8021X";
			else
				value = "NONE";
			break;
		case WPS_AUTHTYPE_WPAPSK:
		case WPS_AUTHTYPE_WPA2PSK:
			if (enabled8021X)
				value = "WPA-PSK IEEE8021X";
			else
				value = "WPA-PSK";
			break;
		case WPS_AUTHTYPE_WPA:
		case WPS_AUTHTYPE_WPA2:
			if (enabled8021X)
				value = "WPA-EAP IEEE8021X";
			else
				value = "WPA-EAP";
			break;
		default:
			value = 0;
			break;
		}
		if (!value | wpa_config_set(ssid, var, value, 0))
			break;

		/* proto */
		var = "proto";
		switch (auth) {
		case WPS_AUTHTYPE_WPA:
		case WPS_AUTHTYPE_WPAPSK:
			value = "WPA";
			break;
		case WPS_AUTHTYPE_WPA2:
		case WPS_AUTHTYPE_WPA2PSK:
			value = "RSN";
			break;
		default:
			ssid->proto = 0;
			value = 0;
			break;
		}
		if (value && wpa_config_set(ssid, var, value, 0))
			break;

		/* pariwise */
		var = "pairwise";
		switch (encr) {
		case WPS_ENCRTYPE_NONE:
			ssid->pairwise_cipher = WPA_CIPHER_NONE;
			value = 0;
			break;
		case WPS_ENCRTYPE_TKIP:
			value = "TKIP";
			break;
		case WPS_ENCRTYPE_AES:
			value = "CCMP";
			break;
		default:
			value = 0;
			break;
		}
		if (value && wpa_config_set(ssid, var, value, 0))
			break;

		/* group */
		var = "group";
		switch (encr) {
		case WPS_ENCRTYPE_NONE:
			ssid->group_cipher = WPA_CIPHER_NONE;
			value = 0;
			break;
		case WPS_ENCRTYPE_WEP:
			value = "WEP104 WEP40";
			break;
		case WPS_ENCRTYPE_TKIP:
			value = "TKIP";
			break;
		case WPS_ENCRTYPE_AES:
                        #if 0   /* original */
			value = "CCMP";
                        #else   /* HACK! */
                        /* It is not uncommon for group cipher to be
                         * TKIP whereas the pairwise is CCMP, but 
                         * WPS makes no distinction.
                         * Workaround by configuring both in this case.
                         */
			value = "TKIP CCMP";
                        #endif
			break;
		default:
			value = 0;
			break;
		}
		if (value && wpa_config_set(ssid, var, value, 0))
			break;

		/* wep_tx_keyidx */
		var = "wep_tx_keyidx";
		switch (encr) {
		case WPS_ENCRTYPE_WEP:
			value = (char *)os_malloc(2);
			if (!value)
				break;
			os_snprintf(value, 2, "%d", nwKeyIdx - 1);
			break;
		default:
			value = 0;
			break;
		}
		if (value && wpa_config_set(ssid, var, value, 0)) {
			free(value);
			break;
		} else if (value)
			free(value);
		if (!value && (WPS_ENCRTYPE_WEP == encr))
			break;

		/* wep_keyn */
		switch (encr) {
		case WPS_ENCRTYPE_WEP:
			var = (char *)os_malloc(9);
			if (!var)
				break;
			os_snprintf(var, 9, "wep_key%d", nwKeyIdx - 1);
			if (is_hex(nwKey, nwKey_length)) {
				value = (char *)os_malloc(nwKey_length * 2 + 1);
				if (!value)
					break;
				wpa_snprintf_hex_uppercase(value, nwKey_length * 2 + 1,
										   nwKey, nwKey_length);
				value[nwKey_length * 2] = 0;
			} else if ((5 == nwKey_length) || (13 == nwKey_length)) {
				value = (char *)os_malloc(nwKey_length + 3);
				if (!value)
					break;
				os_snprintf(value, nwKey_length + 3, "\"%s\"", nwKey);
			} else if ((nwKey_length) || (13 == nwKey_length)) {
				value = (char *)os_malloc(nwKey_length + 1);
				if (!value)
					break;
				os_memcpy(value, nwKey, nwKey_length);
				value[nwKey_length] = 0;
			}
			break;
		default:
			var = 0;
			value = 0;
			break;
		}
		if (var && value && wpa_config_set(ssid, var, value, 0))
			break;
		if (var)
			free(var);
		if (value)
			free(value);
		if ((!var || !value) && (WPS_ENCRTYPE_WEP == encr))
			break;

		/* psk */
		var = "psk";
		switch (auth) {
		case WPS_AUTHTYPE_WPA:
		case WPS_AUTHTYPE_WPA2:
		case WPS_AUTHTYPE_WPAPSK:
		case WPS_AUTHTYPE_WPA2PSK:
			if (nwKey_length) {
				value = (char *)os_malloc(nwKey_length + 3);
				if (!value)
					break;
				if (64 > nwKey_length) {
					os_snprintf(value, nwKey_length + 3, "\"%s\"", nwKey);
					passphrase = 1;
				} else if (64 == nwKey_length) {
					os_memcpy(value, nwKey, nwKey_length);
					value[nwKey_length] = 0;
				} else {
					free(value);
					value = 0;
					break;
				}
			} else
				value = 0;
			break;
		default:
			value = 0;
			break;
		}
		if (value && wpa_config_set(ssid, var, value, 0)) {
			free(value);
			break;
		} else if (value)
			free(value);
		if (nwKey_length && !value &&
			((WPS_AUTHTYPE_WPA == auth) ||
			 (WPS_AUTHTYPE_WPA2 == auth) ||
			 (WPS_AUTHTYPE_WPAPSK == auth) ||
			 (WPS_AUTHTYPE_WPA2PSK == auth)))
			break;
		
		/* eap */
		if (eapType && os_strlen(eapType)) {
			var = "eap";
			value = (char *)eapType;
			if (wpa_config_set(ssid, var, value, 0))
				break;
		}

		/* identity */
		if (eapIdentity && os_strlen(eapIdentity)) {
			var = "identity";
			value = (char *)eapIdentity;
			if (wpa_config_set(ssid, var, value, 0))
				break;
		}

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	if (ret) {
		if (ssid)
			(void)wpa_config_remove_network(wpa_s->conf, ssid->id);
	} else {
		if (passphrase)
			wpa_config_update_psk(ssid);
	}

	if (nwKey)
		free(nwKey);
	if (eapType)
		free(eapType);
	if (eapIdentity)
		free(eapIdentity);

	return ret?ret:ssid->id;
}


struct wpa_scan_result *wps_select_ssid(struct wpa_supplicant *wpa_s,
		struct wpa_scan_result *results,
		int num, struct wpa_ssid **ssid)
{
	struct wpa_scan_result *bss;
	struct wpa_scan_result *selected = 0;
	int num_good;
	struct wpa_scan_result *maybe = 0;
        int num_maybe;
	struct wpa_ssid *tmp;
	int ret = -1;
	char *var, *value;
	struct wps_data *data = 0;
	char bssid_str[18+1];
	char ssid_str[32+3];
    u8 uuidSel[SIZE_UUID];
    u8 uuidBss[SIZE_UUID];
    size_t length;
	struct wps_config *wps;
	int i;
        int is_push_button;

	do {
		if (!wpa_s || !wpa_s->conf || !results || !num || !ssid)
			break;

		wps = wpa_s->conf->wps;
                is_push_button = (wps->dev_pwd_len == 8 && 
                        memcmp(wps->dev_pwd, "00000000", 8) == 0);

		if (wps->nwid_trying_wps != -1) {
			(void)wpa_config_remove_network(wpa_s->conf, wps->nwid_trying_wps);
			wps->nwid_trying_wps = -1;
		}

		*ssid = 0;

		num_good = 0;
		num_maybe = 0;
		for (i = 0; i < num; i++) {
			bss = &results[i];
            if (wps->filter_bssid_flag && memcmp(
                        wps->filter_bssid, bss->bssid, 6) != 0) {
                continue;
            }
            if (wps->filter_ssid_length > 0 && memcmp(
                        wps->filter_ssid, bss->ssid, 
                        wps->filter_ssid_length) != 0) {
                continue;
            }
			if (bss->wps_ie_len) {
				u16 dev_pwd_id;

				do {
					if (wps_create_wps_data(&data))
						break;

					if (wps_parse_wps_ie(bss->wps_ie, bss->wps_ie_len, data))
						break;

		            length = sizeof(uuidBss);
                    /* selected bss UUID-E */
		            if (wps_get_value(data, WPS_TYPE_UUID_E, uuidBss, &length))
			            break;

					if (wps_get_value(data, WPS_TYPE_DEVICE_PWD_ID, &dev_pwd_id, 0))
						break;

                    if (is_push_button) {
                        if (WPS_DEVICEPWDID_PUSH_BTN != dev_pwd_id) 
                            break;
                    } else {        /* PIN mode */
                        if (WPS_DEVICEPWDID_DEFAULT != dev_pwd_id) {
                            break;
                        }
                    }
                    
                    /* In the case of a dual-band AP and a dual-band station,
                     * the station may discover more than one Registrar in PBC mode.
                     * If the dual-band station does discover more than one Registrar
                     * in PBC mode, one each RF band, and the UUID in the Beacon and
                     * Probe-Response are the same for all RF bands, then the station 
                     * shall not consider this to be a session overlap.
                     */
					if (num_good) {
					    /* check dual-band concurrent AP case */
					    if (((selected->freq >= 4000) && (bss->freq <= 3000)) ||
					        ((selected->freq <= 3000) && (bss->freq >= 4000)))
					    {
                    		/* one is 2.4GHz and another is 5GHz */
                    		if (memcmp(uuidSel, uuidBss, length) == 0)
                    		{
                    		    /* UUID-E of them are totally the same */
                    		    /* Select the 5GHz one */
                    		    if (bss->freq >= 4000)
					                selected = bss;
                    		    break;
                    		}
					    }
					}
				    selected = bss;
            		memcpy(&uuidSel, &uuidBss, length);
					num_good++;
				} while (0);

				(void)wps_destroy_wps_data(&data);

                /* If we have WPS at all then it's a maybe
                 * for PIN
                 */
                if (!is_push_button) {
                    num_maybe++;
					if ((!maybe) || (bss->qual > maybe->qual)) {
						maybe = bss;
					}
                }
			} else {
                /* No WPS IE */
                /* If in open mode, it may support WPS...
                 * (some microsoft products)
                 * take it as a maybe
                 */
                if ((bss->caps & IEEE80211_CAP_PRIVACY) == 0) {
                    num_maybe++;
                    if ((!maybe) || (bss->qual > maybe->qual)) {
                        maybe = bss;
                    }
                }
            }
		}

        /* Push button mode must have exact explicit match.
         * For PIN mode, due to greater security, we are willing
         * to be more lenient; and due to the ambiguity of the WPS
         * standard, it is not actually required that the AP
         * advertise it's PIN mode (as versus PB mode
         * which requires such advertisement).
         * But we want either one good match, or if no good match,
         * then just one maybe... otherwise the user should have
         * done pre-filtering.... 
         * for best results, the user should do pre-filtering!
         */
        if (is_push_button) {
		    if (num_good > 1) {
		        wpa_msg(wpa_s, MSG_INFO, 
                        "Must be only one AP enabled WPS-PBC");
                break;
            } else if (num_good == 1) {
                /* good */
            } else if (num_good == 0) {
		        wpa_msg(wpa_s, MSG_INFO, 
                        "There are no APs which are enabled WPS-PBC");
                break;
            }
		} else { /* PIN mode */
		    if (num_good > 1) {
			    wpa_msg(wpa_s, MSG_INFO, 
                        "Must be only one AP enabled WPS-PIN");
                break;
            } else if (num_good == 1) {
                /* good */
            } else if (num_maybe > 1) {
			    wpa_msg(wpa_s, MSG_INFO, 
                        "There are no APs which are enabled WPS-PIN"
                        " but attaching WPS IE, concern some microsoft products,"
                        " try to connect strongest RSSI one");
				selected = maybe;
            } else if (num_maybe == 1) {
                /* select this one */
                selected = maybe;
            } else {
			    wpa_msg(wpa_s, MSG_INFO, 
                        "There are no APs available for WPS");
                break;
            }
        }

		if (!selected)
			break;

		*ssid = wpa_config_add_network(wpa_s->conf);
		if (!*ssid)
			break;
		wpa_config_set_network_defaults(*ssid);
		(*ssid)->disabled = 1;

		/* bssid */
		var = "bssid";
		os_snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
					selected->bssid[0], selected->bssid[1],
					selected->bssid[2], selected->bssid[3],
					selected->bssid[4], selected->bssid[5]);
		bssid_str[18] = 0;
		if (wpa_config_set(*ssid, var, bssid_str, 0))
			break;

		/* ssid */
		var = "ssid";
		ssid_str[0] = '"';
		os_memcpy(ssid_str + 1, selected->ssid, selected->ssid_len);
		ssid_str[selected->ssid_len + 1] = '"';
		ssid_str[selected->ssid_len + 2] = 0;
		if (wpa_config_set(*ssid, var, ssid_str, 0))
			break;

		/* auth_alg */
		var = "auth_alg";
		value = "OPEN";
		if (wpa_config_set(*ssid, var, value, 0))
			break;

		/* key_mgmt */
		var = "key_mgmt";
		value = "IEEE8021X";
		if (wpa_config_set(*ssid, var, value, 0))
			break;

		/* eap */
		var = "eap";
		value = "WPS";
		if (wpa_config_set(*ssid, var, value, 0))
			break;

		/* identity */
		var = "identity";
		free((*ssid)->identity);
		switch (wps->reg_mode) {
		case WPS_SUPPLICANT_REGMODE_NONE:
			value = WPS_IDENTITY_ENROLLEE;
			break;
		default:
			value = WPS_IDENTITY_REGISTRAR;
			break;
		}
		(*ssid)->identity = (u8 *)os_strdup(value);
		(*ssid)->identity_len = os_strlen(value);

		ret = 0;
	} while (0);

	if (ret) {
		if (*ssid)
			(void)wpa_config_remove_network(wpa_s->conf, (*ssid)->id);
		selected = 0;
	} else {
		wpa_msg(wpa_s, MSG_INFO, "Found AP for WPS [%*s]", (*ssid)->ssid_len,(*ssid)->ssid);

		switch (wps->reg_mode) {
		case WPS_SUPPLICANT_REGMODE_NONE:
			if (wps->config) {
				free(wps->config);
				wps->config = 0;
				wps->config_len = 0;
			}
			break;
		case WPS_SUPPLICANT_REGMODE_CONFIGURE_AP:
			break;
		case WPS_SUPPLICANT_REGMODE_REGISTER_AP:
		case WPS_SUPPLICANT_REGMODE_REGISTER_STA:
			if (wps->config) {
				free(wps->config);
				wps->config = 0;
				wps->config_len = 0;
			}

			(void)wps_get_supplicant_ssid_configuration(wpa_s, (*ssid)->id, &wps->config, &wps->config_len);
			break;
		default:
			break;
		}

		if (wpa_s->current_ssid)
			wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);

		tmp = wpa_s->conf->ssid;
		while (tmp) {
                        #if 0   /* original from Sony */
			tmp->disabled = (*ssid)->id != tmp->id;
                        #else
                        /* Set bit 1 of disabled as "temporary WPS bit" */
			tmp->disabled = ((*ssid)->id != tmp->id) << 1;
                        #endif
			tmp = tmp->next;
		}

		wps->nwid_trying_wps = (*ssid)->id;

		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}

	return selected;
}


int wps_config_remove_network(struct wpa_supplicant *wpa_s, int network_id)
{
	if (0 > network_id)
		return -1;
	return wpa_config_remove_network(wpa_s->conf, network_id);
}


int wps_config_create_probe_req_ie(void *ctx, u8 **buf, size_t *len)
{
	int ret = -1;
	struct wpa_supplicant *wpa_s = ctx;
	struct wps_config *wps;
	struct wps_data *wps_ie = NULL;
	u16 uuid_type;
	u8 u8val;
	u16 u16val;
	size_t length;

	do {
		if (!wpa_s || !wpa_s->conf || !buf || !len)
			break;

		*buf = 0;
		*len = 0;

		wps = wpa_s->conf->wps;
		if (!wps)
			break;

		if (wps_create_wps_data(&wps_ie))
			break;

		/* Version */
		if (!wps->version)
			u8val = WPS_VERSION;
		else
			u8val = wps->version;
		if (wps_set_value(wps_ie, WPS_TYPE_VERSION, &u8val, 0))
			break;

		/* Request Type */
		if (WPS_SUPPLICANT_REGMODE_NONE == wps->reg_mode)
			u8val = WPS_REQTYPE_ENROLLEE_INFO_ONLY;
		else
			u8val = WPS_REQTYPE_REGISTRAR;
		if (wps_set_value(wps_ie, WPS_TYPE_REQ_TYPE, &u8val, 0))
			break;

		/* Config Methods */
		if (wps_set_value(wps_ie, WPS_TYPE_CONFIG_METHODS, &wps->config_methods, 0))
			break;

		/* UUID-(E or R) */
		if (!wps->uuid_set)
			break;
		if (wps->reg_mode == WPS_SUPPLICANT_REGMODE_NONE)
			uuid_type = WPS_TYPE_UUID_E;
		else
			uuid_type = WPS_TYPE_UUID_R;
		if (wps_set_value(wps_ie, uuid_type, wps->uuid, sizeof(wps->uuid)))
			break;

		/* Primary Device Type */
		if (wps_set_value(wps_ie, WPS_TYPE_PRIM_DEV_TYPE, wps->prim_dev_type, sizeof(wps->prim_dev_type)))
			break;

		/* RF Bands */
		if (wps_set_value(wps_ie, WPS_TYPE_RF_BANDS, &wps->rf_bands, 0))
			break;

		/* Association State */
		u16val = WPS_ASSOC_NOT_ASSOCIATED;
		if (wps_set_value(wps_ie, WPS_TYPE_ASSOC_STATE, &u16val, 0))
			break;

		/* Configuration Error */
		u16val = WPS_ERROR_NO_ERROR;
		if (wps_set_value(wps_ie, WPS_TYPE_CONFIG_ERROR, &u16val, 0))
			break;

		/* Device Password ID */
		if (wps_set_value(wps_ie, WPS_TYPE_DEVICE_PWD_ID, &wps->dev_pwd_id, 0))
			break;

		length = 0;
		if (wps_write_wps_ie(wps_ie, buf, &length))
			break;
		*len = (size_t)length;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps_ie);

	if (ret) {
		if (buf && *buf) {
			free(*buf);
			*buf = 0;
		}
		if (len)
			*len = 0;
	}

	return ret;
}

int wps_config_create_assoc_req_ie(void *ctx, u8 **buf, size_t *len)
{
	int ret = -1;
	struct wpa_supplicant *wpa_s = ctx;
	struct wps_config *wps;
	struct wps_data *wps_ie = NULL;
	u8 u8val;
	size_t length;

	do {
		if (!wpa_s || !wpa_s->conf || !buf || !len)
			break;

		*buf = 0;
		*len = 0;

		wps = wpa_s->conf->wps;
		if (!wps)
			break;

		if (wps_create_wps_data(&wps_ie))
			break;

		/* Version */
		if (!wps->version)
			u8val = WPS_VERSION;
		else
			u8val = wps->version;
		if (wps_set_value(wps_ie, WPS_TYPE_VERSION, &u8val, 0))
			break;

		/* Request Type */
		if (WPS_SUPPLICANT_REGMODE_NONE == wps->reg_mode)
			u8val = WPS_REQTYPE_ENROLLEE_INFO_ONLY;
		else
			u8val = WPS_REQTYPE_REGISTRAR;
		if (wps_set_value(wps_ie, WPS_TYPE_REQ_TYPE, &u8val, 0))
			break;

		length = 0;
		if (wps_write_wps_ie(wps_ie, buf, &length))
			break;
		*len = (size_t)length;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps_ie);

	if (ret) {
		if (buf && *buf) {
			free(*buf);
			*buf = 0;
		}
		if (len)
			*len = 0;
	}

	return ret;
}


