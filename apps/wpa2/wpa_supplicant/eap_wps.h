/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: eap_wps.h
//  Description: EAP-WPS main source header
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

#ifndef EAP_WPS_H
#define EAP_WPS_H

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

struct wps_config;

struct eap_wps_target_info {
	u8		version;
	u8		uuid[SIZE_UUID];
	int		uuid_set;

	u8		mac[SIZE_MAC_ADDR];
	int		mac_set;

	u16		auth_type_flags;
	u16		encr_type_flags;
	u8		conn_type_flags;
	u16		config_methods;
	u8		wps_state;
	u8		*manufacturer;
	size_t	manufacturer_len;
	u8		*model_name;
	size_t	model_name_len;
	u8		*model_number;
	size_t	model_number_len;
	u8		*serial_number;
	size_t	serial_number_len;
	u8		prim_dev_type[SIZE_8_BYTES];
	u8		*dev_name;
	size_t	dev_name_len;
	u8		rf_bands;
	u16		assoc_state;
	u16		config_error;
	u32		os_version;

	u8		nonce[SIZE_NONCE];
	u8		pubKey[SIZE_PUB_KEY];
	int		pubKey_set;
	u16		dev_pwd_id;
	u8		hash1[SIZE_WPS_HASH];
	u8		hash2[SIZE_WPS_HASH];

	u8		*config;
	size_t	config_len;
};

struct eap_wps_data {
	enum {START, M1, M2, M2D, M3, M4, M5, M6, M7, M8, DONE, ACK, NACK, FAILURE} state;
	enum {NONE, REGISTRAR, ENROLLEE} mode;

	u8		*rcvMsg;
	u32		rcvMsgLen;
	Boolean	fragment;

	u8		*sndMsg;
	u32		sndMsgLen;

	u16		dev_pwd_id;
	u8		dev_pwd[SIZE_64_BYTES];
	u16		dev_pwd_len;

	u16		assoc_state;
	u16		config_error;

	u8		nonce[SIZE_NONCE];
	u8		pubKey[SIZE_PUB_KEY];
	int		preset_pubKey;

	void	*dh_secret;

	u8		authKey[SIZE_AUTH_KEY];
	u8		keyWrapKey[SIZE_KEY_WRAP_KEY];
	u8		emsk[SIZE_EMSK];

	u8		snonce1[SIZE_NONCE];
	u8		snonce2[SIZE_NONCE];
	u8		psk1[SIZE_128_BITS];
	u8		psk2[SIZE_128_BITS];
	u8		hash1[SIZE_WPS_HASH];
	u8		hash2[SIZE_WPS_HASH];

	enum wps_supplicant_reg_mode reg_mode;
	u8		*config;
	size_t	config_len;

	struct eap_wps_target_info *target;
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

int eap_wps_free_dh(void **dh);
int eap_wps_generate_sha256hash(u8 *inbuf, int inbuf_len, u8 *outbuf);
int eap_wps_generate_public_key(void **dh_secret, u8 *public_key);
int eap_wps_generate_device_password_id(u16 *dev_pwd_id);
int eap_wps_generate_device_password(u8 *dev_pwd, int dev_pwd_len);
struct eap_wps_enable_params {
        /* passed to eap_wps_enable() */
        u8 *dev_pwd;    /* 00000000 for push button method */
        int dev_pwd_len;
        int filter_bssid_flag;   /* accept only given bssid? */
        u8  *filter_bssid;     /* 6 bytes; used if filter_bssid_flag */
        int filter_ssid_length;  /* accept only given essid? */
        u8  *filter_ssid;
        int seconds_timeout;    /* 0 for default timeout, -1 for no timeout */
        enum wps_config_who config_who;
        int do_save;            /* for WPS_CONFIG_WHO_ME */
};
int eap_wps_enable(struct wpa_supplicant *wpa_s, struct wps_config *wps,
                struct eap_wps_enable_params *param);
int eap_wps_disable(struct wpa_supplicant *wpa_s, struct wps_config * wps);
int eap_wps_done_delayed(struct wpa_supplicant *wpa_s, struct wps_config * wps);

int eap_wps_device_password_validation(const u8 *pwd, const int len);

int eap_wps_config_init_data(struct wps_config *conf,
							 struct eap_wps_data *data);
void eap_wps_config_deinit_data(struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M1(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M1(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M2(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
u8 * eap_wps_config_build_message_M2D(struct wps_config *conf,
									  struct eap_wps_data *data,
									  size_t *msg_len);
int eap_wps_config_process_message_M2(struct wps_config *conf,
									  struct eap_wps_data *data,
									  Boolean *with_config);
int eap_wps_config_process_message_M2D(struct wps_config *conf,
									   struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M3(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M3(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M4(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M4(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M5(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M5(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M6(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M6(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M7(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M7(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M8(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M8(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 * eap_wps_config_build_message_special(struct wps_config *conf,
										  struct eap_wps_data *data,
										  u8 msg_type,
										  u8 *e_nonce, u8 *r_nonce,
										  size_t *msg_len);
int eap_wps_config_process_message_special(struct wps_config *conf,
										   struct eap_wps_data *data,
										   u8 msg_type,
										   u8 *e_nonce, u8 *r_nonce);

int eap_wps_config_select_ssid_configuration(struct wps_config *conf,
											 struct eap_wps_data *data,
											 u8 *raw_data, size_t raw_data_len,
											 Boolean wrap_credential);
int eap_wps_config_set_ssid_configuration(struct wps_config *conf,
										  void *wpa_s,
										  u8 *raw_data, size_t raw_data_len,
										  Boolean wrap_credential);

#endif /* EAP_WPS_H */
