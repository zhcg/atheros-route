/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: eap_wps.c
//  Description: EAP-WPS main source
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
#include "eloop.h"
#include "wps_config.h"
#include "wpa_supplicant_i.h"
#include "wpa_supplicant.h"
#include "eap_i.h"
#include "eap_wps.h"
#include "wps_parser.h"
#include "wpa_ctrl.h"
#include "wpa.h"
#include "config.h"

#ifdef CONFIG_CRYPTO_INTERNAL

#include "crypto.h"
#include "sha256.h"
#include "os.h"
/* openssl provides RAND_bytes; os_get_random is equivalent */
#define RAND_bytes(buf,n) os_get_random(buf,n)

#else   /* CONFIG_CRYPTO_INTERNAL */

#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#endif  /* CONFIG_CRYPTO_INTERNAL */


#define EAP_OPCODE_WPS_START	0x01
#define EAP_OPCODE_WPS_ACK		0x02
#define EAP_OPCODE_WPS_NACK		0x03
#define EAP_OPCODE_WPS_MSG		0x04
#define EAP_OPCODE_WPS_DONE		0x05
#define EAP_OPCODE_WPS_FLAG_ACK	0x06

#define EAP_FLAG_MF	0x01
#define EAP_FLAG_LF	0x02

#define EAP_VENDOR_ID_WPS	"\x00\x37\x2a"
#define EAP_VENDOR_TYPE_WPS	"\x00\x00\x00\x01"

/* Polling period */
#define EAP_WPS_PERIOD_SEC		1
#define EAP_WPS_PERIOD_USEC		0
/* Timeout period */
#define EAP_WPS_TIMEOUT_SEC		120
#define EAP_WPS_TIMEOUT_USEC	        0
/* End cleanup timeout period */
#define EAP_WPS_CLEANUP_TIMEOUT_SEC	2
#define EAP_WPS_CLEANUP_TIMEOUT_USEC	0

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

const static u8 DH_P_VALUE[SIZE_1536_BITS] = 
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
    0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
    0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
    0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
    0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
    0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
    0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
    0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
    0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
    0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
    0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
    0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
    0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
    0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
    0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
    0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
    0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
    0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
    0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
    0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
    0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x23, 0x73, 0x27,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#ifdef CONFIG_CRYPTO_INTERNAL
const static u8 DH_G_VALUE[] = { 2 };
#else   /* CONFIG_CRYPTO_INTERNAL */
const static u32 DH_G_VALUE = 2;
#endif  /* CONFIG_CRYPTO_INTERNAL */

struct eap_format {
	u8 type;
	u8 vendor_id[3];
	u8 vendor_type[4];
	u8 op_code;
	u8 flags;
};

u8 credential_number = 0;

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

static int eap_wps_clear_target_info(struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;

	do {
		if (!data || !data->target)
			break;

		target = data->target;

		if (target->manufacturer)
			os_free(target->manufacturer);
		if (target->model_name)
			os_free(target->model_name);
		if (target->model_number)
			os_free(target->model_number);
		if (target->serial_number)
			os_free(target->serial_number);
		if (target->dev_name)
			os_free(target->dev_name);
		if (target->config) {
			os_free(target->config);
			target->config = 0;
			target->config_len = 0;
		}

		os_memset(target, 0, sizeof(*target));
		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_config_init_data(struct wps_config *conf, struct eap_wps_data *data)
{
	int ret = -1;

	do {
		if (!conf || !data)
			break;

		data->target = wpa_zalloc(sizeof(*data->target));
		if (!data->target)
			break;

		if (conf->dev_pwd_len) {
			data->dev_pwd_id = conf->dev_pwd_id;
			os_memcpy(data->dev_pwd, conf->dev_pwd, conf->dev_pwd_len);
			data->dev_pwd_len = conf->dev_pwd_len;
		}

		if (conf->set_pub_key) {
			os_memcpy(data->pubKey, conf->pub_key, sizeof(data->pubKey));
			if (conf->dh_secret)
				data->dh_secret = conf->dh_secret;
			data->preset_pubKey = 1;
		}

		if (conf->config) {
			data->config = (u8 *)os_malloc(conf->config_len);
			if (!data->config)
				break;
			os_memcpy(data->config, conf->config, conf->config_len);
			data->config_len = conf->config_len;
		}

		data->reg_mode = conf->reg_mode;

		data->state = START;
		ret = 0;
	} while (0);

	return ret;
}


static int eap_wps_init_data(struct eap_sm *sm, struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
        #if 0   /* original sony */
	const u8 *pwd;
	size_t pwd_len;
        #endif

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_init_data(conf, data))
			break;

                #if 0   /* original sony code: uses wpa passphrase for wps password !??! */
		pwd = eap_get_config_password(sm, &pwd_len);
		if (pwd && pwd_len) {
			if (pwd_len > sizeof(data->dev_pwd))
				pwd_len = sizeof(data->dev_pwd);

			if (8 == pwd_len) {
				if (eap_wps_device_password_validation(pwd, (int)pwd_len))
					data->dev_pwd_id = WPS_DEVICEPWDID_USER_SPEC;
				else
					data->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
			} else
				data->dev_pwd_id = WPS_DEVICEPWDID_USER_SPEC;
			os_memcpy(data->dev_pwd, pwd, pwd_len);
			data->dev_pwd_len = pwd_len;

			if (data->preset_pubKey) {
				os_memset(data->pubKey, 0, sizeof(data->pubKey));
				data->dh_secret = 0;
				data->preset_pubKey = 0;
			}
		}
                #endif  /* original sony code */

		ret = 0;
	} while (0);

	return ret;
}


static void *eap_wps_init(struct eap_sm *sm)
{
	int result = -1;
	struct eap_wps_data *data;

	do {
		data = wpa_zalloc(sizeof(*data));
		if (data == NULL)
			break;

		if (eap_wps_init_data(sm, data))
			break;

		sm->eap_method_priv = data;
		result = 0;
	} while (0);

	if (result) {
		os_free(data);
		data = 0;
	}

	return data;
}


void eap_wps_config_deinit_data(struct eap_wps_data *data)
{
	do {
		if (!data)
			break;

		if (data->rcvMsg) {
			os_free(data->rcvMsg);
			data->rcvMsg = 0;
			data->rcvMsgLen = 0;
			data->fragment = 0;
		}

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}

		if (!data->preset_pubKey && data->dh_secret) {
                        #if 1   /* Atheros */
			eap_wps_free_dh((void **)&data->dh_secret);
                        #else   /* from Sony */
			DH_free(data->dh_secret);
                        #endif
			data->dh_secret = 0;
		}

		if (data->config) {
			os_free(data->config);
			data->config = 0;
			data->config_len = 0;
		}

		if (data->target) {
			eap_wps_clear_target_info(data);
			os_free(data->target);
			data->target = 0;
		}

		os_free(data);
	} while (0);
}

static void eap_wps_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_wps_data *data = (struct eap_wps_data *)priv;
	if (data == NULL)
		return;

	eap_wps_config_deinit_data(data);
}


static void eap_wps_request(struct eap_sm *sm,
							int req_type, const char *msg, size_t msg_len)
{
#define CTRL_REQ_TYPE_COMP			0
#define CTRL_REQ_TYPE_FAIL			1
#define CTRL_REQ_TYPE_PASSWORD		2
#define CTRL_REQ_TYPE_DONE              3
	char *buf;
	size_t buflen;
	int len = 0;
	char *field;
	char *txt;

	if (sm == NULL)
		return;

	switch(req_type) {
	case CTRL_REQ_TYPE_COMP:
                /* NOTE! as expected by wpatalk........... */
		field = "EAP-WPS-SUCCESS";
		txt = "Complete EAP-WPS protocol";
		break;
	case CTRL_REQ_TYPE_FAIL:
		field = "EAP-WPS-FAIL";
		txt = "Fail EAP-WPS protocl";
		break;
	case CTRL_REQ_TYPE_PASSWORD:
		field = "EAP-WPS-PASSWORD";
		txt = "Request Password for EAP-WPS";
		break;
        case CTRL_REQ_TYPE_DONE:
                /* NOTE! as expected by wpatalk........... */
                field = "WPS-JOB-DONE";
                txt = "WPS no longer ready";
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
		os_free(buf);
		return;
	}
	buf[buflen - 1] = '\0';
	wpa_msg(sm->msg_ctx, MSG_INFO, "%s", buf);
	os_free(buf);
}


int eap_wps_generate_sha256hash(u8 *inbuf, int inbuf_len, u8 *outbuf)
{
	int ret = -1;

	do {
		if (!inbuf || !inbuf_len || !outbuf)
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL
                {
                        const u8 *vec[1];
                        size_t vlen[1];
                        vec[0] = inbuf;
                        vlen[0] = inbuf_len;
                        sha256_vector(1, vec, vlen, outbuf);
                }
                #else /* CONFIG_CRYPTO_INTERNAL */
		if (!SHA256(inbuf, inbuf_len, outbuf))
			break;
                #endif /* CONFIG_CRYPTO_INTERNAL */

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_free_dh(void **dh)
{
	int ret = -1;
	do {
		if (!dh || !*dh)
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL
                os_free(*dh);
                *dh = NULL;
                #else /* CONFIG_CRYPTO_INTERNAL */
		DH_free(*dh);
		*dh = 0;
                #endif /* CONFIG_CRYPTO_INTERNAL */

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_generate_public_key(void **dh_secret, u8 *public_key)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        if (dh_secret) *dh_secret = NULL;

	do {
                size_t len;
		if (!dh_secret || !public_key)
			break;

                /* We here generate both private key and public key.
                * For compatibility with the openssl version of code
                * (from Sony), dh_secret retains the private key
                * it is NOT the Diffie-Helman shared secret!).
                * The private key is used later to generate various other
                * data that can be decrypted by recipient using the public key.
                */
                *dh_secret = os_malloc(SIZE_PUB_KEY);
                if (dh_secret == NULL) break;
                RAND_bytes(*dh_secret, SIZE_PUB_KEY);  /* make private key */
                len = SIZE_PUB_KEY;
                if (crypto_mod_exp(
                        DH_G_VALUE,
                        sizeof(DH_G_VALUE),
                        *dh_secret,     /* private key */
                        SIZE_PUB_KEY,
                        DH_P_VALUE,
                        sizeof(DH_P_VALUE),
                        public_key,     /* output */
                        &len            /* note: input/output */
                        ) ) break;
                if (0 < len && len < SIZE_PUB_KEY) {
                        /* Convert to fixed size big-endian integer */
                        memmove(public_key+(SIZE_PUB_KEY-len),
                            public_key, len);
                        memset(public_key, 0, (SIZE_PUB_KEY-len));
                } else
                if (len != SIZE_PUB_KEY) 
                        break;
                ret = 0;
        } while (0);

        if (ret) {
            if (dh_secret && *dh_secret) os_free(*dh_secret);
            if (dh_secret) *dh_secret = NULL;
        }

        #else   /* CONFIG_CRYPTO_INTERNAL */

	u8 tmp[SIZE_PUB_KEY];
	DH *dh = 0;
	u32 g;
	int length;

	do {
		if (!dh_secret || !public_key)
			break;

		*dh_secret = 0;

		dh = DH_new();
		if(!dh)
			break;

		dh->p = BN_new();
		if (!dh->p)
			break;

		dh->g = BN_new();
		if (!dh->g)
			break;
	   
		if(!BN_bin2bn(DH_P_VALUE, SIZE_1536_BITS, dh->p))
			break;

		g = host_to_be32(DH_G_VALUE);
		if(!BN_bin2bn((u8 *)&g, 4, dh->g))
			break;

		if(!DH_generate_key(dh))
			break;

		length = BN_bn2bin(dh->pub_key, tmp);
		if (!length)
			break;

		length = BN_bn2bin(dh->pub_key, public_key);
                if (0 < length && length < SIZE_PUB_KEY) {
                        /* Convert to fixed size big-endian integer */
                        memmove(public_key+(SIZE_PUB_KEY-length),
                            public_key, length);
                        memset(public_key, 0, (SIZE_PUB_KEY-length));
                } else
                if (length != SIZE_PUB_KEY)
                        break;
		ret = 0;
	} while (0);

	if (ret && dh) {
		DH_free(dh);
	} else if (dh) {
		*dh_secret = dh;
	}

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_generate_kdk(struct eap_wps_data *data, u8 *e_nonce, u8 *mac,
								u8 *r_nonce, u8 *kdk)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	do {
	        u8 *dh_secret = data->dh_secret;  /* actually, is private key*/
                u8 dhkey[SIZE_DHKEY/*32 bytes*/];
	        u8 shared_secret[SIZE_PUB_KEY];  /* the real DH Shared Secret*/
                const u8 *vec[3];
                size_t vlen[3];

		if (!dh_secret || !e_nonce || !mac || !r_nonce || !kdk)
			break;

                /* Calculate the Diffie-Hellman shared secret g^AB mod p
                * by calculating (PKr)^A mod p
                * (For compatibility with Sony code, dh_secret is NOT
                * the Diffie-Hellman Shared Secret but instead contains
                * just the private key).
                */
                size_t len = SIZE_PUB_KEY;
                if (crypto_mod_exp(
                        data->target->pubKey,
                        SIZE_PUB_KEY,
                        dh_secret,              /* our private key */
                        SIZE_PUB_KEY,
                        DH_P_VALUE,
                        sizeof(DH_P_VALUE),
                        shared_secret,         /* output */
                        &len               /* in/out */
                        )) break;
                if (0 < len && len < SIZE_PUB_KEY) {
                        /* Convert to fixed size big-endian integer */
                        memmove(shared_secret+(SIZE_PUB_KEY-len),
                            shared_secret, len);
                        memset(shared_secret, 0, (SIZE_PUB_KEY-len));
                } else if (len != SIZE_PUB_KEY) 
                        break;

                /* Calculate DHKey (hash of DHSecret)
                */
                vec[0] = shared_secret;
                vlen[0] = SIZE_PUB_KEY;  /* DH Secret size, 192 bytes */
                sha256_vector(
                        1,  // num_elem
                        vec,
                        vlen,
                        dhkey   /* output: 32 bytes */
                        );

                /* Calculate KDK (Key Derivation Key)
                */
                vec[0] = e_nonce;
                vlen[0] = SIZE_NONCE;
                vec[1] = mac;
                vlen[1] = SIZE_MAC_ADDR;
                vec[2] = r_nonce;
                vlen[2] = SIZE_NONCE;
                hmac_sha256_vector(
                        dhkey,
                        SIZE_DHKEY,
                        3,              /* num_elem */
                        vec,
                        vlen,
                        kdk     /* output: 32 bytes */
                        );
                ret = 0;
        } while (0);

        #else   /* CONFIG_CRYPTO_INTERNAL */

	DH *dh_secret = (DH *)data->dh_secret;
	BIGNUM *bn_peer = 0;
	u8 shared_secret[SIZE_PUB_KEY];
	int shared_secret_length;
	u8 sec_key_sha[SIZE_256_BITS];
	u8 kdk_src[SIZE_NONCE + SIZE_MAC_ADDR + SIZE_NONCE];
	int kdk_src_len;

	do {
		if (!dh_secret || !e_nonce || !mac || !r_nonce || !kdk)
			break;

		bn_peer = BN_new();
		if (!bn_peer)
			break;

		if (!BN_bin2bn(data->target->pubKey, SIZE_PUB_KEY, bn_peer))
			break;

		shared_secret_length = DH_compute_key(shared_secret, bn_peer, dh_secret);
                if (0 < shared_secret_length && shared_secret_length < SIZE_PUB_KEY) {
                        /* Convert to fixed size big-endian integer */
                        memmove(shared_secret+(SIZE_PUB_KEY-shared_secret_length),
                            shared_secret, shared_secret_length);
                        memset(shared_secret, 0, (SIZE_PUB_KEY-shared_secret_length));
                } else
                if (shared_secret_length != SIZE_PUB_KEY)
                        break;

		if (!SHA256(shared_secret, shared_secret_length, sec_key_sha))
			break;

		kdk_src_len = 0;
		os_memcpy((u8 *)kdk_src + kdk_src_len, e_nonce, SIZE_NONCE);
		kdk_src_len += SIZE_NONCE;
		os_memcpy((u8 *)kdk_src + kdk_src_len, mac, SIZE_MAC_ADDR);
		kdk_src_len += SIZE_MAC_ADDR;
		os_memcpy((u8 *)kdk_src + kdk_src_len, r_nonce, SIZE_NONCE);
		kdk_src_len += SIZE_NONCE;
		if (!HMAC(EVP_sha256(), sec_key_sha, SIZE_256_BITS,
				  kdk_src, kdk_src_len, kdk, NULL))
			break;

		ret = 0;
	} while (0);

	if (bn_peer)
		BN_free(bn_peer);

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_key_derive_func(struct eap_wps_data *data, 
						   u8 *kdk,
						   u8 keys[KDF_OUTPUT_SIZE])
{
        const char *personalization = WPS_PERSONALIZATION_STRING;
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	do {
                const u8 *vec[3];
                size_t vlen[3];
                u8 cb1[4];
                u8 cb2[4];
                int iter;

		WPA_PUT_BE32(cb2, KDF_KEY_BITS/*== 640*/);
                vec[0] = cb1;   /* Note: cb1 modified in loop below */
                vlen[0] = sizeof(cb1);
                vec[1] = (void *)personalization;
                vlen[1] = os_strlen(personalization);
                vec[2] = cb2;
                vlen[2] = sizeof(cb2);

                for (iter = 0; iter < KDF_N_ITERATIONS; iter++) {
		        WPA_PUT_BE32(cb1, iter+1);
                        hmac_sha256_vector(
                                kdk,
                                SIZE_KDK,
                                3,      /* num_elem */
                                vec,
                                vlen,
                                keys + SHA256_MAC_LEN*iter  /* out: 32 bytes/iteration */
                                );
                }
                ret = 0;
        } while (0);

        #else   /* CONFIG_CRYPTO_INTERNAL */

	u8 *prf;
	u32 prf_len;
	u8 *hmac = 0, *pos;
	u32 hmac_len = 0, length = 0;
	u32 i;

	do {
		prf_len = sizeof(u32) + os_strlen(personalization) + sizeof(u32);
		prf = os_malloc(prf_len);
		if (!prf)
			break;

		pos = prf + sizeof(u32);
		os_memcpy(pos, personalization, os_strlen(personalization));
		pos += os_strlen(personalization);
		WPA_PUT_BE32(pos, KDF_KEY_BITS);

		for (i = 1; i <= KDF_N_ITERATIONS; i++) {
			WPA_PUT_BE32(prf, i);
			length = 0;
			(void)HMAC(EVP_sha256(), kdk, SIZE_256_BITS, prf, prf_len, 0, &length);
			hmac = (u8 *)os_realloc(hmac, hmac_len + length);
			pos = hmac + hmac_len;
			if (!HMAC(EVP_sha256(), kdk, SIZE_256_BITS, prf, prf_len, pos, &length))
				break;
			hmac_len += length;
		}
		if (i <= KDF_N_ITERATIONS)
			break; 
		if ((KDF_KEY_BITS / 8) > hmac_len)
			break;

		if (!keys)
			break;
		os_memcpy(keys, hmac, KDF_KEY_BITS / 8);

		ret = 0;
	} while (0);

	if (prf)
		os_free(prf);
	if (hmac)
		os_free(hmac);

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_hmac_validation(struct eap_wps_data *data,
	   u8 *authenticator, u8 *auth_key)
{
	int ret = -1;

        #ifndef CONFIG_CRYPTO_INTERNAL
	u8 *hmac_src = 0;
	u32 hmac_src_len;
        #endif
	struct wps_data *wps = 0;
	u8 *buf = 0;
	size_t buf_len;
	u8 hmac[SIZE_256_BITS];

	do {
		if (!data || !authenticator || !auth_key)
			break;

                /* Atheros note: this Sony code goes to a lot of extra effort 
                 * to parse the data, remove the authenticator and then
                 * recreate the original packet minus the authenticator...
                 * not necessary since the authenticator will always
                 * be at the end... so it could be optimized...
                 */

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		if (wps_remove_value(wps, WPS_TYPE_AUTHENTICATOR))
			break;

		if (wps_write_wps_data(wps, &buf, &buf_len))
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL

                {
                        const u8 *vec[2];
                        size_t vlen[2];
                        vec[0] = data->sndMsg;
                        vlen[0] = data->sndMsgLen;
                        vec[1] = buf;
                        vlen[1] = buf_len;
                        hmac_sha256_vector(
                            auth_key,
                            SIZE_AUTH_KEY,
                            2,  /* num_elem */
                            vec,
                            vlen,
                            hmac);
                }

                #else   /* CONFIG_CRYPTO_INTERNAL */

		hmac_src_len = data->sndMsgLen + buf_len;
		hmac_src = os_malloc(hmac_src_len);
		if (!hmac_src)
			break;

		os_memcpy(hmac_src, data->sndMsg, data->sndMsgLen);
		os_memcpy(hmac_src + data->sndMsgLen, buf, buf_len);

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS, hmac_src, hmac_src_len, hmac, NULL))
			break;

                #endif   /* CONFIG_CRYPTO_INTERNAL */

		if (os_memcmp(hmac, authenticator, SIZE_64_BITS))
			break;

		ret = 0;
	} while (0);

        #ifndef CONFIG_CRYPTO_INTERNAL
	if (hmac_src)
		os_free(hmac_src);
        #endif
	if (buf)
		os_free(buf);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_encrypt_data(struct eap_wps_data *data,
								u8 *inbuf, int inbuf_len,
								u8 *encrKey,
								u8 *iv, u8 **cipher, int *cipher_len)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        void *aesHandle = NULL;

        if (cipher) *cipher = NULL;
        do {
                u8 *lastcipher;
                u8 *thiscipher;
                aesHandle = aes_encrypt_init(encrKey, ENCR_DATA_BLOCK_SIZE);

		RAND_bytes(iv, ENCR_DATA_BLOCK_SIZE);
                lastcipher = iv;

		if (!cipher || !cipher_len)
			break;

                /* The output is up to one block larger than the input */
                *cipher = os_malloc(inbuf_len+ENCR_DATA_BLOCK_SIZE);
                *cipher_len = 0;
                thiscipher = *cipher;
                for (;; ) {
                        u8 block[ENCR_DATA_BLOCK_SIZE];
                        int i;
                        int thislen = inbuf_len;
                        if (thislen > ENCR_DATA_BLOCK_SIZE)
                                thislen = ENCR_DATA_BLOCK_SIZE;
                        if (thislen > 0) 
                                memcpy(block, inbuf, thislen );
                        if (thislen < ENCR_DATA_BLOCK_SIZE) {
                                /* Last block: 
                                 * pad out with a byte value that gives the 
                                 * number of padding bytes.
                                 */
                                int npad = ENCR_DATA_BLOCK_SIZE - thislen;
                                int ipad;
                                for (ipad = 0; ipad < npad; ipad++) {
                                        block[ENCR_DATA_BLOCK_SIZE-ipad-1] = 
                                                npad;
                                }
                        }
                        /* Cipher Block Chaining (CBC) -- 
                         * xor the plain text with the last AES output
                         * (or initially, the "initialization vector").
                         */
                        for (i = 0; i < ENCR_DATA_BLOCK_SIZE; i++) {
                                block[i] ^= lastcipher[i];
                        }
                        /* And encrypt and store in output */
                        aes_encrypt(aesHandle, block, thiscipher);
                        lastcipher = thiscipher;
                        thiscipher += ENCR_DATA_BLOCK_SIZE;
                        *cipher_len += ENCR_DATA_BLOCK_SIZE;
                        if ( thislen < ENCR_DATA_BLOCK_SIZE ) {
                                ret = 0;
                                break;
                        }
                        inbuf += ENCR_DATA_BLOCK_SIZE;
                        inbuf_len -= ENCR_DATA_BLOCK_SIZE;
                }
        } while (0);
        if (aesHandle) aes_encrypt_deinit(aesHandle);

        #else   /* CONFIG_CRYPTO_INTERNAL */

	EVP_CIPHER_CTX ctx;
	u8 buf[1024];
	int buf_len;
	int length, curr_len; int block_size;

        if (cipher) *cipher = NULL;
	do {
		RAND_bytes(iv, SIZE_128_BITS);

		if (!cipher || !cipher_len)
			break;

		if (!EVP_EncryptInit(&ctx, EVP_aes_128_cbc(), encrKey, iv))
			break;

		length = inbuf_len;
		block_size = sizeof(buf) - SIZE_128_BITS;

		*cipher = 0;
		*cipher_len  = 0;
		while (length) {
			if (length > block_size)
				curr_len = block_size;
			else
				curr_len = length;

			if (!EVP_EncryptUpdate(&ctx, buf, &buf_len, inbuf, curr_len))
				break;
			*cipher = (u8 *)os_realloc(*cipher, *cipher_len + buf_len);
			os_memcpy(*cipher + *cipher_len, buf, buf_len);
			*cipher_len += buf_len;
			length -= curr_len;
		}

		if (length)
			break;

		if (!EVP_EncryptFinal(&ctx, buf, &buf_len))
			break;

		*cipher = (u8 *)os_realloc(*cipher, *cipher_len + buf_len);
		os_memcpy(*cipher + *cipher_len, buf, buf_len);
		*cipher_len += buf_len;

		ret = 0;
	} while (0);

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	if (ret) {
		if (cipher_len)
			*cipher_len = 0;
		if (cipher && *cipher) {
			os_free(*cipher);
			*cipher = 0;
		}
	}

	return ret;
}


static int eap_wps_decrypt_data(struct eap_wps_data *data, u8 *iv,
								u8 *cipher, int cipher_len,
								u8 *encrKey, u8 **plain, int *plain_len)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        void *aesHandle = NULL;
        if (plain) *plain = NULL;

	do {
                u8 *out;
                int out_len = 0;

		if (!iv || !cipher || !encrKey || !plain || !plain_len)
			break;
                if (cipher_len <= 0 || 
                            (cipher_len & (ENCR_DATA_BLOCK_SIZE-1)) != 0) 
                        break;

                /* The plain text length is always less than the cipher
                 * text length (which contains 1 to 16 bytes of padding).
                 * No harm in allocating more than we need.
                 */
		*plain = os_malloc(cipher_len);
		*plain_len = 0;
                if (*plain == NULL) break;
                out = *plain;

                aesHandle = aes_decrypt_init(encrKey, ENCR_DATA_BLOCK_SIZE);
                if (aesHandle == NULL) break;

                while (cipher_len >= ENCR_DATA_BLOCK_SIZE) {
                        int block_len = ENCR_DATA_BLOCK_SIZE;
                        int i;
                        aes_decrypt(aesHandle, cipher, out);
                        /* Cipher Block Chaining (CBC) -- xor the plain text with
                         * the last AES output (or initially, the "initialization vector").
                         */
                        for (i = 0; i < ENCR_DATA_BLOCK_SIZE; i++) {
                                out[i] ^= iv[i];
                        }
                        iv = cipher;
                        cipher += ENCR_DATA_BLOCK_SIZE;
                        cipher_len -= ENCR_DATA_BLOCK_SIZE;
                        if (cipher_len < ENCR_DATA_BLOCK_SIZE) {
                                int npad;
                                /* cipher_len should be exactly 0
                                 * at this point... it must be a multiple
                                 * of blocks.  The last block should contain
                                 * between 1 and 16 bytes of padding,
                                 * with the last byte of padding saying
                                 * how many.
                                 */
                                if (cipher_len != 0) break;
                                npad = out[ENCR_DATA_BLOCK_SIZE-1];
                                if (npad > 0 && npad <= ENCR_DATA_BLOCK_SIZE) {
                                        block_len -= npad;
                                } else goto bad;
                        }
                        out += block_len;
                        out_len += block_len;
                }
                *plain_len = out_len;
                ret = 0;
                break;
        } while (0);
        bad:
        if (aesHandle) aes_decrypt_deinit(aesHandle);

        #else /* CONFIG_CRYPTO_INTERNAL */

	EVP_CIPHER_CTX ctx;
	u8 buf[1024];
	int buf_len = sizeof(buf);
	int length, curr_len;
	int block_size;

	do {
		if (!iv || !cipher || !encrKey || !plain || !plain_len)
			break;

		*plain = 0;
		*plain_len = 0;

		if (!EVP_DecryptInit(&ctx, EVP_aes_128_cbc(), encrKey, iv))
			break;

		length = cipher_len;
		block_size = sizeof(buf) - SIZE_128_BITS;

		while (length) {
			if (length > block_size)
				curr_len = block_size;
			else
				curr_len = length;

			if (!EVP_DecryptUpdate(&ctx, buf, &buf_len, cipher, curr_len))
				break;
			*plain = (u8 *)os_realloc(*plain, *plain_len + buf_len);
			os_memcpy(*plain + *plain_len, buf, buf_len);
			*plain_len += buf_len;
			length -= curr_len;
		}

		if (length)
			break;

		if (!EVP_DecryptFinal(&ctx, buf, &buf_len))
			break;

		*plain = (u8 *)os_realloc(*plain, *plain_len + buf_len);
		os_memcpy(*plain + *plain_len, buf, buf_len);
		*plain_len += buf_len;

		ret = 0;
	} while (0);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	if (ret) {
		if (plain_len)
			*plain_len = 0;
		if (plain && *plain) {
			os_free(*plain);
			*plain = 0;
		}
	}

	return ret;
}


static int eap_wps_encrsettings_creation(struct eap_wps_data *data,
										 u16 nonce_type, u8 *nonce,
										 u8 *buf, size_t buf_len,
										 u8 *auth_key, u8 *key_wrap_auth,
										 u8 **encrs, int *encrs_len)
{
	int ret = -1;
	struct wps_data *wps = 0;
	u8 hmac[SIZE_256_BITS];
	size_t length = 0;
	u8 *tmp = 0;
	u8 *cipher = 0, iv[SIZE_128_BITS];
	int cipher_len;

	do {
		if (!auth_key || !key_wrap_auth || !encrs || !encrs_len)
			break;

		*encrs = 0;
		*encrs_len = 0;

		if (wps_create_wps_data(&wps))
			break;

		if (nonce) {
			length = SIZE_NONCE;
			if (wps_set_value(wps, nonce_type, nonce, length))
				break;

			length = 0;
			if (wps_write_wps_data(wps, &tmp, &length))
				break;
		}

		if (buf && buf_len) {
			(void)wps_destroy_wps_data(&wps);

			tmp = os_realloc(tmp, length + buf_len);
			if (!tmp)
				break;
			os_memcpy(tmp + length, buf, buf_len);
			length += buf_len;

			if (wps_create_wps_data(&wps))
				break;

			if (wps_parse_wps_data(tmp, length, wps))
				break;
		}

                #ifdef CONFIG_CRYPTO_INTERNAL

                {
                        const u8 *vec[1];
                        size_t vlen[1];
                        vec[0] = tmp;
                        vlen[0] = length;
                        hmac_sha256_vector(
                                auth_key,
                                SIZE_AUTH_KEY,  /* auth_key size */
                                1,              /* num_elem */
                                vec,
                                vlen,
                                hmac     /* output: 32 bytes */
                                );
                }

                #else /* CONFIG_CRYPTO_INTERNAL */

		if (!HMAC(EVP_sha256(), auth_key, SIZE_AUTH_KEY, tmp, length, hmac, NULL))
			break;

                #endif /* CONFIG_CRYPTO_INTERNAL */

		if (wps_set_value(wps, WPS_TYPE_KEY_WRAP_AUTH, hmac, SIZE_64_BITS))
			break;

		os_free(tmp);
		tmp = 0;

		length = 0;
		if (wps_write_wps_data(wps, &tmp, &length))
			break;

		if (eap_wps_encrypt_data(data, tmp, length, key_wrap_auth, iv, &cipher, &cipher_len))
			break;

		*encrs = os_malloc(SIZE_128_BITS + cipher_len);
		if (!*encrs)
			break;
		os_memcpy(*encrs, iv, SIZE_128_BITS);
		os_memcpy(*encrs + SIZE_128_BITS, cipher, cipher_len);
		*encrs_len = SIZE_128_BITS + cipher_len;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (cipher)
		os_free(cipher);

	if (ret) {
		if (encrs_len)
			*encrs_len = 0;
		if (encrs && *encrs) {
			os_free(*encrs);
			*encrs = 0;
		}
	}

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_encrsettings_validation(struct eap_wps_data *data,
										   u8 *plain, int plain_len,
										   u8 *auth_key, u16 nonce_type,
										   u8 *nonce, u8 *key_wrap_auth)
{
	int ret = -1;
	struct wps_data *wps = 0;
	size_t length;
	u8 *buf = 0;
	u8 hmac[SIZE_256_BITS];

	do {
		if (!plain || !plain_len || !key_wrap_auth)
			break;
		
		if (wps_create_wps_data(&wps))
			break;
		if (wps_parse_wps_data(plain, plain_len, wps))
			break;

		if (nonce) {
		/* Nonce */
			length = SIZE_NONCE;
			if (wps_get_value(wps, nonce_type, nonce, &length))
				break;
		}

		/* Key Wrap Authenticator */
		length = SIZE_8_BYTES;
		if (wps_get_value(wps, WPS_TYPE_KEY_WRAP_AUTH, key_wrap_auth, &length))
			break;

		if (wps_remove_value(wps, WPS_TYPE_KEY_WRAP_AUTH))
			break;

		length = 0;
		if (wps_write_wps_data(wps, &buf, &length))
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL

                {
                        const u8 *vec[1];
                        size_t vlen[1];
                        vec[0] = buf;
                        vlen[0] = length;
                        hmac_sha256_vector(
                                auth_key,
                                SIZE_AUTH_KEY,  /* auth_key size */
                                1,              /* num_elem */
                                vec,
                                vlen,
                                hmac     /* output: 32 bytes */
                                );
                }

                #else /* CONFIG_CRYPTO_INTERNAL */

		if (!HMAC(EVP_sha256(), auth_key, SIZE_AUTH_KEY, buf, length, hmac, NULL))
			break;

                #endif /* CONFIG_CRYPTO_INTERNAL */

		if (os_memcmp(hmac, key_wrap_auth, SIZE_64_BITS))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	if (ret) {
		if (nonce)
			os_memset(nonce, 0, SIZE_NONCE);
		if (key_wrap_auth)
			os_memset(key_wrap_auth, 0, SIZE_8_BYTES);
	}

	return ret;
}


static int eap_wps_generate_hash(struct eap_wps_data *data,
		 u8 *src, int src_len,
		 u8 *pub_key1, u8 *pub_key2,
		 u8 *auth_key,
		 u8 *psk, u8 *es, u8 *hash)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	do {
                const u8 *vec[4];
                size_t vlen[4];
	        u8 hash_tmp[SHA256_MAC_LEN];

		if (!src || !pub_key1 || !pub_key2 || !psk || !es || !auth_key)
			break;

                /* Generate psk1 or psk2 while we are at it 
                 * (based on parts of the wps password == PIN) 
                 */
                vec[0] = src;
                vlen[0] = src_len;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_AUTH_KEY,
                        1,              /* num_elem */
                        vec,
                        vlen,
                        hash_tmp     /* output: 32 bytes */
                        );
		os_memcpy(psk, hash_tmp, SIZE_128_BITS); /* first 16 bytes */

                /* Generate a nonce while we are at it */
		RAND_bytes(es, SIZE_128_BITS);

                /* Generate hash (includes above nonce and psk portion) */
                vec[0] = es;
                vlen[0] = SIZE_128_BITS;
                vec[1] = psk;
                vlen[1] = SIZE_128_BITS;        /* first 16 bytes only */
                vec[2] = pub_key1;
                vlen[2] = SIZE_PUB_KEY;
                vec[3] = pub_key2;
                vlen[3] = SIZE_PUB_KEY;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_AUTH_KEY,  /* auth_key size */
                        4,              /* num_elem */
                        vec,
                        vlen,
                        hash     /* output: 32 bytes */
                        );
		ret = 0;
	} while (0);

        #else /* CONFIG_CRYPTO_INTERNAL */

	u8 hash_tmp[SIZE_256_BITS];
	u8 hash_src[SIZE_128_BITS * 2 + SIZE_PUB_KEY * 2];
	u8 *tmp;

	do {
		if (!src || !pub_key1 || !pub_key2 || !psk || !es || !auth_key)
			break;

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS, src, src_len,
			 hash_tmp, NULL))
			break;
		os_memcpy(psk, hash_tmp, SIZE_128_BITS);

		RAND_bytes(es, SIZE_128_BITS);

		tmp = hash_src;
		os_memcpy(tmp, es, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, psk, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, pub_key1, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;
		os_memcpy(tmp, pub_key2, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS,
				  hash_src, tmp - hash_src, hash, NULL))
			break;

		ret = 0;
	} while (0);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


int eap_wps_generate_device_password_id(u16 *dev_pwd_id)
{
	int ret = -1;

	do {
		if (!dev_pwd_id)
			break;

		RAND_bytes((u8 *)dev_pwd_id, 2);
		*dev_pwd_id |= 0x8000;
		*dev_pwd_id &= 0xfff0;

		ret = 0;
	} while (0);

	return ret;
}


static u8 eap_wps_compute_device_password_checksum(u32 pin)
{
	u32 acc = 0;
	u32 tmp = pin * 10;

	acc += 3 * ((tmp / 10000000) % 10);
	acc += 1 * ((tmp / 1000000) % 10);
	acc += 3 * ((tmp / 100000) % 10);
	acc += 1 * ((tmp / 10000) % 10);
	acc += 3 * ((tmp / 1000) % 10);
	acc += 1 * ((tmp / 100) % 10);
	acc += 3 * ((tmp / 10) % 10);

	return (u8)(10 - (acc % 10)) % 10;
}


int eap_wps_generate_device_password(u8 *dev_pwd, int dev_pwd_len)
{
	int ret = -1;

	do {
		if (!dev_pwd || !dev_pwd_len)
			break;

		RAND_bytes(dev_pwd, dev_pwd_len);
		if (8 == dev_pwd_len) {
			u32 val;
			u8 check_sum, tmp[9];
			val = *(u32 *)dev_pwd;
			check_sum = eap_wps_compute_device_password_checksum(val);
			val = val * 10 + check_sum;
			os_snprintf((char *)tmp, 9, "%08u", val);
			os_memcpy(dev_pwd, tmp, 8);
		}

		ret = 0;
	} while (0);

	return ret;
}


static int eap_wps_oobdevpwd_public_key_hash_validation(const u8 *hashed, const u8 *raw)
{
	int ret = -1;
	u8 src[SIZE_256_BITS];

	do {
		if (!hashed || !raw)
			break;

		if (eap_wps_generate_sha256hash((u8 *)raw, SIZE_PUB_KEY, src))
			break;

		if (os_memcmp(hashed, src, SIZE_20_BYTES))
			break;

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_device_password_validation(const u8 *pwd, const int len)
{
	int ret = -1;
	u32 pin;
	char str_pin[9], *end;
	u8 check_sum;

	do {
		if (!pwd || 8 != len)
			break;

		os_memcpy(str_pin, pwd, 8);
		str_pin[8] = 0;
		pin = strtoul(str_pin, &end, 10);
		if (end != (str_pin + 8))
			break;

		check_sum = eap_wps_compute_device_password_checksum(pin / 10);
		if (check_sum != (u8)(pin % 10))
			break;

		ret = 0;
	} while (0);

	return ret;
}


static int eap_wps_calcurate_authenticator(struct eap_wps_data *data,
										   u8 *sndmsg, size_t sndmsg_len,
										   u8 *auth_key, u8 *authenticator)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	u8 hmac[SIZE_256_BITS];

	do {
                const u8 *vec[2];
                size_t vlen[2];

		if (!data || !sndmsg || !authenticator)
			break;

                vec[0] = data->rcvMsg;
                vlen[0] = data->rcvMsgLen;
                vec[1] = sndmsg;
                vlen[1] = sndmsg_len;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_256_BITS,  /* auth_key size */
                        2,              /* num_elem */
                        vec,
                        vlen,
                        hmac     /* output: 32 bytes */
                        );
		os_memcpy(authenticator, hmac, SIZE_64_BITS);
		ret = 0;
	} while (0);

        #else /* CONFIG_CRYPTO_INTERNAL */

	u8 *hmac_src = 0;
	int hmac_src_len;
	u8 hmac[SIZE_256_BITS];

	do {
		if (!data || !sndmsg || !authenticator)
			break;

		hmac_src_len = data->rcvMsgLen + sndmsg_len;
		hmac_src = os_malloc(hmac_src_len);
		os_memcpy(hmac_src, data->rcvMsg, data->rcvMsgLen);
		os_memcpy(hmac_src + data->rcvMsgLen, sndmsg, sndmsg_len);

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS,
				  hmac_src, hmac_src_len, hmac, NULL))
			break;

		os_memcpy(authenticator, hmac, SIZE_64_BITS);

		ret = 0;
	} while (0);

	if (hmac_src)
		os_free(hmac_src);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_hash_validation(struct eap_wps_data *data,
								   u8 *compared,
								   u8 *rsnonce, u8 *psk,
								   u8 *pub_key1, u8 *pub_key2,
								   u8 *auth_key)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        do {
	        u8 target[SIZE_256_BITS];
                const u8 *vec[4];
                size_t vlen[4];

		if (!compared || !rsnonce || !psk || !pub_key1 || !pub_key2 || !auth_key)
			break;

                vec[0] = rsnonce;
                vlen[0] = SIZE_128_BITS;
                vec[1] = psk;
                vlen[1] = SIZE_128_BITS;
                vec[2] = pub_key1;
                vlen[2] = SIZE_PUB_KEY;
                vec[3] = pub_key2;
                vlen[3] = SIZE_PUB_KEY;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_256_BITS,  /* auth_key size */
                        4,              /* num_elem */
                        vec,
                        vlen,
                        target     /* output: 32 bytes */
                        );

		if (os_memcmp(compared, target, SIZE_256_BITS))
			break;

                ret = 0;
        } while (0);

        #else /* CONFIG_CRYPTO_INTERNAL */

	u8 hash_src[SIZE_128_BITS * 2 + SIZE_PUB_KEY * 2];
	u8 *tmp;
	u8 target[SIZE_256_BITS];

	do {
		if (!compared || !rsnonce || !psk || !pub_key1 || !pub_key2 || !auth_key)
			break;

		tmp = hash_src;
		os_memcpy(tmp, rsnonce, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, psk, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, pub_key1, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;
		os_memcpy(tmp, pub_key2, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS, hash_src, tmp - hash_src, target, NULL))
			 	break;

		if (os_memcmp(compared, target, SIZE_256_BITS))
			break;

		ret = 0;
	} while (0);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


int eap_wps_config_select_ssid_configuration(struct wps_config *conf,
											 struct eap_wps_data *data,
											 u8 *raw_data, size_t raw_data_len,
											 Boolean wrap_credential)
{
	int ret = -1;
	struct wps_data *wps = 0;
	u8 *val = 0;
	size_t val_len;
	u8 id;
	int index = -1;


	do {
		if (!conf || !data || !raw_data || !raw_data_len)
			break;

		/* Creadential */
		val_len = 0;
		if (wrap_credential) {
			if (wps_create_wps_data(&wps))
				break;

			if (wps_parse_wps_data(raw_data, raw_data_len, wps))
				break;

			(void)wps_get_value(wps, WPS_TYPE_CREDENTIAL, 0, &val_len);
			if (!val_len)
				break;
			val = (u8 *)os_malloc(val_len);
			if (!val)
				break;
			if (wps_get_value(wps, WPS_TYPE_CREDENTIAL, val, &val_len))
				break;

			(void)wps_destroy_wps_data(&wps);
		} else {
			val = os_malloc(raw_data_len);
			if (!val)
				break;
			os_memcpy(val, raw_data, raw_data_len);
			val_len = raw_data_len;
		}

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(val, val_len, wps))
			break;

		if (wps_get_value(wps, WPS_TYPE_NW_INDEX, &id, 0))
			break;

		index = (int)id;
		ret = 0;
	} while (0);

	if (val)
		os_free(val);
	(void)wps_destroy_wps_data(&wps);

	if (ret && (0 <= index))
		index = -1;

	return index;
}


static int eap_wps_select_ssid_configuration(struct eap_sm *sm,
											 struct eap_wps_data *data,
											 u8 *raw_data, size_t raw_data_len,
											 Boolean wrap_credential)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	int index;
	char msg[10];

	do {
		if (!data || !raw_data || !raw_data_len || !conf)
			break;

		index = eap_wps_config_select_ssid_configuration(conf, data,
														 raw_data, raw_data_len,
														 wrap_credential);
		if (0 > index)
			break;

		os_snprintf(msg, sizeof(msg), "%d", index);
		eap_wps_request(sm, CTRL_REQ_TYPE_COMP, msg, os_strlen(msg));
		eap_wps_request(sm, CTRL_REQ_TYPE_DONE, 0, 0);

		ret = 0;
	} while (0);

	return ret;
}


static void eap_wps_reconfigure_callback(void *eloop_ctx, void *user_ctx)
{
	struct wpa_global *global = eloop_ctx;
        struct wpa_supplicant *wpa_s = global->ifaces;
        struct wpa_supplicant *except_wpa_s = user_ctx;
	wpa_printf(MSG_DEBUG, "Reloading all other configurations");
        while (wpa_s != NULL) {
                if (wpa_s != except_wpa_s &&
                                wpa_supplicant_reload_configuration(wpa_s)) {
                        wpa_printf(MSG_ERROR, 
                                "Terminating due to config reload failure");
			eloop_terminate();
                }
                wpa_s = wpa_s->next;
        }
}




int eap_wps_config_set_ssid_configuration(struct wps_config *conf,
                                          void *ctx,
                                          u8 *raw_data_org, size_t raw_data_org_len,
                                          Boolean wrap_credential)
{
    int ret = -1;
    struct wps_data *wps = 0;
    u8 *val = 0, *wpaTkip = 0, *wpa2Tkip = 0, *wpaAes = 0, *raw_data = 0;
    u16 encr, auth;
    size_t val_len, wpaTkip_len = 0, wpa2Tkip_len = 0, wpaAes_len = 0, raw_data_len;
    int index = -1;
    struct wpa_supplicant *wpa_s = ctx;

    credential_number = 0;
    raw_data = raw_data_org;
    raw_data_len = raw_data_org_len;

    do {
        if (!conf || !wpa_s || !raw_data || !raw_data_len)
            break;

        /* Creadential */
        val_len = 0;
        if (wrap_credential) {
            u8 nwIdx;

            if (wps_create_wps_data(&wps))
                break;

            if (wps_parse_wps_data(raw_data, raw_data_len, wps))
                break;

            (void)wps_get_value(wps, WPS_TYPE_CREDENTIAL, 0, &val_len);
            if (!val_len)
                break;
            val = (u8 *)os_malloc(val_len);
            if (!val)
                break;
            if (wps_get_value(wps, WPS_TYPE_CREDENTIAL, val, &val_len))
                break;

            (void)wps_destroy_wps_data(&wps);

            if (wps_create_wps_data(&wps))
                break;

            if (wps_parse_wps_data(val, val_len, wps))
                break;

            /* Network Index */
            if (!wps_get_value(wps, WPS_TYPE_NW_INDEX, &nwIdx, NULL))
                /** Ignore Network Index **/
                (void)wps_remove_value(wps, WPS_TYPE_NW_INDEX);

            os_free(val);
            val = 0;
            val_len = 0;

            if (wps_get_value(wps, WPS_TYPE_AUTH_TYPE, &auth, NULL))
                break;

            if (wps_get_value(wps, WPS_TYPE_ENCR_TYPE, &encr, NULL))
                break;

            if (wps_write_wps_data(wps, &val, &val_len))
                break;
        } else {
            val = (u8 *)os_malloc(raw_data_len);
            if (!val)
                break;
            os_memcpy(val, raw_data, raw_data_len);
            val_len = raw_data_len;
            //set encr to be NONE such that setting will be always stored.
            encr = WPS_ENCRTYPE_NONE;
            auth = WPS_AUTHTYPE_OPEN;
        }

        if ((encr == WPS_ENCRTYPE_TKIP) && (auth == WPS_AUTHTYPE_WPAPSK))
        {
            if (wpaTkip)
            {
                if (wpaAes)
                {
                    index = wps_set_supplicant_ssid_configuration(wpa_s, wpaAes, wpaAes_len);

                    // reset val_bak value
                    os_free(wpaAes);
                    wpaAes = 0;
                    wpaAes_len = 0;
                }

                if (wpa2Tkip)
                {
                    index = wps_set_supplicant_ssid_configuration(wpa_s, wpa2Tkip, wpa2Tkip_len);

                    // reset val_bak value
                    os_free(wpa2Tkip);
                    wpa2Tkip = 0;
                    wpa2Tkip_len = 0;
                }

                index = wps_set_supplicant_ssid_configuration(wpa_s, wpaTkip, wpaTkip_len);

                // reset val_bak value
                os_free(wpaTkip);
                wpaTkip = 0;
                wpaTkip_len = 0;
            }
            wpaTkip = val;
            wpaTkip_len = val_len;
            val = 0;
        }
        else if ((encr == WPS_ENCRTYPE_TKIP) && (auth == WPS_AUTHTYPE_WPA2PSK))
        {
            if (wpa2Tkip)
            {
                if (wpaAes)
                {
                    index = wps_set_supplicant_ssid_configuration(wpa_s, wpaAes, wpaAes_len);

                    // reset val_bak value
                    os_free(wpaAes);
                    wpaAes = 0;
                    wpaAes_len = 0;
                }

                index = wps_set_supplicant_ssid_configuration(wpa_s, wpa2Tkip, wpa2Tkip_len);

                // reset val_bak value
                os_free(wpa2Tkip);
                wpa2Tkip = 0;
                wpa2Tkip_len = 0;
            }
            wpa2Tkip = val;
            wpa2Tkip_len = val_len;
            val = 0;
        }
        else if ((encr == WPS_ENCRTYPE_AES) && (auth == WPS_AUTHTYPE_WPAPSK))
        {
            if (wpaAes)
            {
                index = wps_set_supplicant_ssid_configuration(wpa_s, wpaAes, wpaAes_len);

                // reset val_bak value
                os_free(wpaAes);
                wpaAes = 0;
                wpaAes_len = 0;
            }       
            wpaAes = val;
            wpaAes_len = val_len;
            val = 0;
        }
        else
        {
            index = wps_set_supplicant_ssid_configuration(wpa_s, val, val_len);
            if (0 > index)
                break;

            // reset val value
            os_free(val);
            val = 0;
        }

        credential_number++;

        raw_data_len -= (val_len + 4 + 5);
        if (raw_data_len > 0)
            raw_data = &raw_data[val_len + 4 + 5];
        else
            break;
    } while (1);
		
    if (wpaAes)
    {
        index = wps_set_supplicant_ssid_configuration(wpa_s, wpaAes, wpaAes_len);

        // reset val_bak value
        os_free(wpaAes);
        wpaAes = 0;
        wpaAes_len = 0;
    }

    if (wpa2Tkip)
    {
        index = wps_set_supplicant_ssid_configuration(wpa_s, wpa2Tkip, wpa2Tkip_len);

        // reset val_bak value
        os_free(wpa2Tkip);
        wpa2Tkip = 0;
        wpa2Tkip_len = 0;
    }

    if (wpaTkip)
    {
        index = wps_set_supplicant_ssid_configuration(wpa_s, wpaTkip, wpaTkip_len);

        // reset val_bak value
        os_free(wpaTkip);
        wpaTkip = 0;
        wpaTkip_len = 0;
    }
    
    /* Reassociate... or do we need to do this later instead? */
    wpa_s->disconnected = 0;
    wpa_s->reassociate = 1;
    ret = 0;

    /* Optionally save back to configuration file */
    if (conf->do_save) {
        ret = wpa_config_write(wpa_s->confname, wpa_s->conf);
        if (ret) {
            wpa_printf(MSG_ERROR, 
                       "Failed to update configuration");
        } else {
            wpa_printf(MSG_DEBUG, "Configuration updated");
            /* Reload other interfaces that might be using
             * the same configuration file.
             * But don't do it right now, because it
             * confuses the eap state machine.
             * And don't do us, because it runs
             * afoul of the tail end of the state
             * machine.... ugh, this seems fragile.
             * (Fortunately we don't need to reload us;
             * we just wrote our configuration to the file!)
             */
            eloop_register_timeout(1/*seconds*/, 0/*usec*/,
                                   eap_wps_reconfigure_callback,
                                   wpa_s->global, wpa_s);
        }
        conf->do_save = 0;
    }

    (void)wps_destroy_wps_data(&wps);

    if (ret && (0 <= index))
        index = -1;

    return index;
}


static int eap_wps_set_supplicant_ssid_configuration(struct eap_sm *sm,
										  struct eap_wps_data *data,
										  u8 *raw_data, size_t raw_data_len,
										  Boolean wrap_credential)
{
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	int index = -1;
	char msg[10];


	do {
		if (!sm || !data || !raw_data || !raw_data_len || !conf)
			break;

		index = eap_wps_config_set_ssid_configuration(conf,
													  sm->msg_ctx,
													  raw_data, raw_data_len,
													  wrap_credential);
		if (0 > index)
			break;

		os_snprintf(msg, 10, "%d", index);
		eap_wps_request(sm, CTRL_REQ_TYPE_COMP, msg, os_strlen(msg));
		eap_wps_request(sm, CTRL_REQ_TYPE_DONE, 0, 0);
	} while (0);

	return index;
}


u8 *eap_wps_config_build_message_M1(struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	u8 u8val;
	size_t length;

	do {
		if (!conf || !msg_len)
			break;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M1;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* UUID-E */
		if (!conf->uuid_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_UUID_E, conf->uuid, sizeof(conf->uuid)))
			break;

		/* MAC Address */
		if (!conf->mac_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_MAC_ADDR, conf->mac, sizeof(conf->mac)))
			break;

		/* Enrollee Nonce */
		RAND_bytes(data->nonce, sizeof(data->nonce));
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, data->nonce, sizeof(data->nonce)))
			break;

		/* Public Key */
		if (!data->preset_pubKey) {
			if (data->dh_secret)
				eap_wps_free_dh((void **)&data->dh_secret);
			if (eap_wps_generate_public_key(&data->dh_secret, data->pubKey))
				break;
		}
		if (wps_set_value(wps, WPS_TYPE_PUBLIC_KEY, data->pubKey, sizeof(data->pubKey)))
			break;

		/* Authentication Type Flags */
		if (wps_set_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &conf->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_set_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &conf->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_set_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &conf->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_METHODS, &conf->config_methods, 0))
			break;

		/* Wi-Fi Protected Setup State */
		if (wps_set_value(wps, WPS_TYPE_WPSSTATE, &conf->wps_state, 0))
			break;

		/* Manufacturer */
		if (wps_set_value(wps, WPS_TYPE_MANUFACTURER, conf->manufacturer, conf->manufacturer_len))
			break;

		/* Model Name */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NAME, conf->model_name, conf->model_name_len))
			break;

		/* Model Number */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NUMBER, conf->model_number, conf->model_number_len))
			break;

		/* Serial Number */
		if (wps_set_value(wps, WPS_TYPE_SERIAL_NUM, conf->serial_number, conf->serial_number_len))
			break;

		/* Primary Device Type */
		if (wps_set_value(wps, WPS_TYPE_PRIM_DEV_TYPE, conf->prim_dev_type, sizeof(conf->prim_dev_type)))
			break;

		/* Device Name */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_NAME, conf->dev_name, conf->dev_name_len))
			break;

		/* RF Bands */
		if (wps_set_value(wps, WPS_TYPE_RF_BANDS, &conf->rf_bands, 0))
			break;

		/* Association State */
		if (wps_set_value(wps, WPS_TYPE_ASSOC_STATE, &data->assoc_state, 0))
			break;

		/* Device Passwork ID */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_PWD_ID, &data->dev_pwd_id, 0))
			break;

		/* Configuration Error */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_ERROR, &data->config_error, 0))
			break;

		/* OS Version */
		if (wps_set_value(wps, WPS_TYPE_OS_VERSION, &conf->os_version, 0))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M1(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M1(conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M1(struct wps_config *conf,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_data *wps = 0;
	u8 msg_type;
	struct eap_wps_target_info *target;
	size_t length;

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		eap_wps_clear_target_info(data);

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &target->version, 0))
			break;
		if ((target->version != WPS_VERSION) && (target->version != WPS_VERSION_EX))
			break;

		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M1)
			break;

		/* UUID-E */
		length = sizeof(target->uuid);
		if (wps_get_value(wps, WPS_TYPE_UUID_E, target->uuid, &length))
			break;

		/* MAC Address */
		length = sizeof(target->mac);
		if (wps_get_value(wps, WPS_TYPE_MAC_ADDR, target->mac, &length))
			break;
		target->mac_set = 1;

		/* Enrollee Nonce */
		length = sizeof(target->nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, &length))
			break;

		/* Public Key */
		length = sizeof(target->pubKey);
		if (wps_get_value(wps, WPS_TYPE_PUBLIC_KEY, target->pubKey, &length))
			break;
                if (0 < length && length < SIZE_PUB_KEY) {
                        /* Defensive programming in case other side omitted
                        *   leading zeroes 
                        */
                        memmove(target->pubKey+(SIZE_PUB_KEY-length), 
                            target->pubKey, length);
                        memset(target->pubKey, 0, (SIZE_PUB_KEY-length));
                } else if (length != SIZE_PUB_KEY)
                        break;
		if (data->preset_pubKey) {
			if (eap_wps_oobdevpwd_public_key_hash_validation(data->pubKey, target->pubKey))
				break;

			os_memset(data->pubKey, 0, sizeof(data->pubKey));
			data->preset_pubKey = 0;
		}

		/* Authentication Type Flags */
		if (wps_get_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &target->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_get_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &target->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_get_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &target->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_METHODS, &target->config_methods, 0))
			break;

		/* Manufacturer */
		(void)wps_get_value(wps, WPS_TYPE_MANUFACTURER, 0, &length);
		if (!length)
			break;
		target->manufacturer = os_malloc(length);
		target->manufacturer_len = length;
		if (wps_get_value(wps, WPS_TYPE_MANUFACTURER, target->manufacturer, &length))
			break;

		/* Model Name */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NAME, 0, &length);
		if (!length)
			break;
		target->model_name = os_malloc(length);
		target->model_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NAME, target->model_name, &length))
			break;

		/* Model Number */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, 0, &length);
		if (!length)
			break;
		target->model_number = os_malloc(length);
		target->model_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, target->model_number, &length))
			break;

		/* Serial Number */
		(void)wps_get_value(wps, WPS_TYPE_SERIAL_NUM, 0, &length);
		if (!length)
			break;
		target->serial_number = os_malloc(length);
		target->serial_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_SERIAL_NUM, target->serial_number, &length))
			break;

		/* Primary Device Type */
		length = sizeof(target->prim_dev_type);
		if (wps_get_value(wps, WPS_TYPE_PRIM_DEV_TYPE, target->prim_dev_type, &length))
			break;

		/* Device Name */
		(void)wps_get_value(wps, WPS_TYPE_DEVICE_NAME, 0, &length);
		if (!length)
			break;
		target->dev_name = os_malloc(length);
		target->dev_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_DEVICE_NAME, target->dev_name, &length))
			break;

		/* RF Bands */
		if (wps_get_value(wps, WPS_TYPE_RF_BANDS, &target->rf_bands, 0))
			break;

		/* Association State */
		if (wps_get_value(wps, WPS_TYPE_ASSOC_STATE, &target->assoc_state, 0))
			break;

		/* Configuration Error */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
			break;

		/* OS Version */
		if (wps_get_value(wps, WPS_TYPE_OS_VERSION, &target->os_version, 0))
			break;

		/* Wi-Fi Protected Setup State */
		if (wps_get_value(wps, WPS_TYPE_WPSSTATE, &target->wps_state, 0))
			break;

		ret = 0;
	} while (0);

	if (ret)
		eap_wps_clear_target_info(data);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M1(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!data || !conf)
			break;

		if (eap_wps_config_process_message_M1(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


static int eap_wps_config_build_message_M2_M2D(struct wps_config *conf,
											   struct eap_wps_data *data,
											   struct wps_data *wps)
{
	int ret = -1;

	do {
		if (!conf || !data || !wps)
			break;

		/* Authentication Type Flags */
		if (wps_set_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &conf->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_set_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &conf->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_set_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &conf->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_METHODS, &conf->config_methods, 0))
			break;

		/* Manufacturer */
		if (wps_set_value(wps, WPS_TYPE_MANUFACTURER, conf->manufacturer, conf->manufacturer_len))
			break;

		/* Model Name */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NAME, conf->model_name, conf->model_name_len))
			break;

		/* Model Number */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NUMBER, conf->model_number, conf->model_number_len))
			break;

		/* Serial Number */
		if (wps_set_value(wps, WPS_TYPE_SERIAL_NUM, conf->serial_number, conf->serial_number_len))
			break;

		/* Primary Device Type */
		if (wps_set_value(wps, WPS_TYPE_PRIM_DEV_TYPE, conf->prim_dev_type, sizeof(conf->prim_dev_type)))
			break;

		/* Device Name */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_NAME, conf->dev_name, conf->dev_name_len))
			break;

		/* RF Bands */
		if (wps_set_value(wps, WPS_TYPE_RF_BANDS, &conf->rf_bands, 0))
			break;

		/* Association State */
		if (wps_set_value(wps, WPS_TYPE_ASSOC_STATE, &data->assoc_state, 0))
			break;

		/* Configuration Error */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_ERROR, &data->config_error, 0))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M2(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 kdk[SIZE_256_BITS];
	u8 keys[KDF_OUTPUT_SIZE];
	u8 authenticator[SIZE_8_BYTES];
	u8 u8val;
	size_t length;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M2;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Registrar Nonce */
		RAND_bytes(data->nonce, sizeof(data->nonce));
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, data->nonce, sizeof(data->nonce)))
			break;

		/* UUID-R */
		if (!conf->uuid_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_UUID_R, conf->uuid, sizeof(conf->uuid)))
			break;

		/* Public Key */
		if (!data->preset_pubKey) {
			if (data->dh_secret)
				eap_wps_free_dh(&data->dh_secret);
			if (eap_wps_generate_public_key(&data->dh_secret, data->pubKey))
				break;
		}
		if (wps_set_value(wps, WPS_TYPE_PUBLIC_KEY, data->pubKey, sizeof(data->pubKey)))
			break;

		/* M2/M2D common data */
		if (eap_wps_config_build_message_M2_M2D(conf, data, wps))
			break;

		/* Device Password ID */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_PWD_ID, &data->dev_pwd_id, 0))
			break;

		/* OS Version */
		if (wps_set_value(wps, WPS_TYPE_OS_VERSION, &conf->os_version, 0))
			break;

		/* Encrypted Settings */
#if 0
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, encrs_len))
			break;
#endif

		/* Generate KDK */
		if (!target->mac_set)
			break;
		if (eap_wps_generate_kdk(data, target->nonce, target->mac, data->nonce, kdk))
			break;

		/* Key Derivation Function */
		if (eap_wps_key_derive_func(data, kdk, keys))
			break;
		os_memcpy(data->authKey, keys, SIZE_256_BITS);
		os_memcpy(data->keyWrapKey, keys + SIZE_256_BITS, SIZE_128_BITS);
		os_memcpy(data->emsk, keys + SIZE_256_BITS + SIZE_128_BITS, SIZE_256_BITS);
                /* last 16 bytes are unused */

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M2(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M2(conf, data, msg_len);
	} while (0);

	return msg;
}


u8 *eap_wps_config_build_message_M2D(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 u8val;
	size_t length;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M2D;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Registrar Nonce */
		RAND_bytes(data->nonce, sizeof(data->nonce));
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, data->nonce, sizeof(data->nonce)))
			break;

		/* UUID-R */
		if (!conf->uuid_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_UUID_R, conf->uuid, sizeof(conf->uuid)))
			break;

		/* M2/M2D common data */
		if (eap_wps_config_build_message_M2_M2D(conf, data, wps))
			break;

		/* OS Version */
		if (wps_set_value(wps, WPS_TYPE_OS_VERSION, &conf->os_version, 0))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M2D(struct eap_sm *sm,
									 struct eap_wps_data *data,
									 size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M2D(conf, data, msg_len);
	} while (0);

	return msg;
}


static int eap_wps_config_process_message_M2_M2D(struct wps_config *conf,
	       struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 tmp[SIZE_64_BYTES];
	size_t length;

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		eap_wps_clear_target_info(data);

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &target->version, 0))
			break;
		if ((target->version != WPS_VERSION) && (target->version != WPS_VERSION_EX))
			break;

		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if ((msg_type != WPS_MSGTYPE_M2) && (msg_type != WPS_MSGTYPE_M2D))
			break;

		/* Enrollee Nonce */
		length = sizeof(tmp);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, tmp, &length))
			break;
		if (os_memcmp(data->nonce, tmp, sizeof(data->nonce)))
			break;

		/* Registrar Nonce */
		length = sizeof(target->nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, &length))
			break;

		/* UUID-R */
		length = sizeof(target->uuid);
		if (wps_get_value(wps, WPS_TYPE_UUID_R, target->uuid, &length))
			break;

		/* Authentication Type Flags */
		if (wps_get_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &target->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_get_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &target->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_get_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &target->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_METHODS, &target->config_methods, 0))
			break;

		/* Manufacturer */
		(void)wps_get_value(wps, WPS_TYPE_MANUFACTURER, 0, &length);
		if (!length)
			break;
		target->manufacturer = os_malloc(length);
		target->manufacturer_len = length;
		if (wps_get_value(wps, WPS_TYPE_MANUFACTURER, target->manufacturer, &length))
			break;

		/* Model Name */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NAME, 0, &length);
		if (!length)
			break;
		target->model_name = os_malloc(length);
		target->model_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NAME, target->model_name, &length))
			break;

		/* Model Number */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, 0, &length);
		if (!length)
			break;
		target->model_number = os_malloc(length);
		target->model_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, target->model_number, &length))
			break;

		/* Serial Number */
		(void)wps_get_value(wps, WPS_TYPE_SERIAL_NUM, 0, &length);
		if (!length)
			break;
		target->serial_number = os_malloc(length);
		target->serial_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_SERIAL_NUM, target->serial_number, &length))
			break;

		/* Primary Device Type */
		length = sizeof(target->prim_dev_type);
		if (wps_get_value(wps, WPS_TYPE_PRIM_DEV_TYPE, target->prim_dev_type, &length))
			break;

		/* Device Name */
		(void)wps_get_value(wps, WPS_TYPE_DEVICE_NAME, 0, &length);
		if (!length)
			break;
		target->dev_name = os_malloc(length);
		target->dev_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_DEVICE_NAME, target->dev_name, &length))
			break;

		/* RF Bands */
		if (wps_get_value(wps, WPS_TYPE_RF_BANDS, &target->rf_bands, 0))
			break;

		/* Association State */
		if (wps_get_value(wps, WPS_TYPE_ASSOC_STATE, &target->assoc_state, 0))
			break;

		/* Configuration Error */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
			break;

		/* OS Version */
		if (wps_get_value(wps, WPS_TYPE_OS_VERSION, &target->os_version, 0))
			break;

		ret = 0;
	} while (0);

	if (ret)
		eap_wps_clear_target_info(data);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


int eap_wps_config_process_message_M2(struct wps_config *conf,
		struct eap_wps_data *data,
		Boolean *with_config)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 kdk[SIZE_256_BITS];
	u8 keys[KDF_OUTPUT_SIZE];
	size_t length;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (with_config)
			*with_config = 0;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M2)
			break;

		if (eap_wps_config_process_message_M2_M2D(conf, data))
			break;

		/* Public Key */
		length = sizeof(target->pubKey);
		if (wps_get_value(wps, WPS_TYPE_PUBLIC_KEY, target->pubKey, &length))
			break;
                if (0 < length && length < SIZE_PUB_KEY) {
                        /* Defensive programming in case other side omitted
                        *   leading zeroes 
                        */
                        memmove(target->pubKey+(SIZE_PUB_KEY-length), 
                            target->pubKey, length);
                        memset(target->pubKey, 0, (SIZE_PUB_KEY-length));
                } else if (length != SIZE_PUB_KEY)
                        break;

		/* Device Password ID */
		if (wps_get_value(wps, WPS_TYPE_DEVICE_PWD_ID, &target->dev_pwd_id, 0))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* Generate KDK */
		if (eap_wps_generate_kdk(data, data->nonce, conf->mac, target->nonce, kdk))
			break;

		/* Key Derivation Function */
		if (eap_wps_key_derive_func(data, kdk, keys))
			break;
		os_memcpy(data->authKey, keys, SIZE_256_BITS);
		os_memcpy(data->keyWrapKey, keys + SIZE_256_BITS, SIZE_128_BITS);
		os_memcpy(data->emsk, keys + SIZE_256_BITS + SIZE_128_BITS, SIZE_256_BITS);
                /* last 16 bytes are unused */

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (length) {
			u8 *encrs = 0;
			u8 *iv, *cipher;
			int cipher_len;
			u8 *config = 0;
			int config_len;
			int fail = 1;

			do {
				encrs = os_malloc(length);
				if (!encrs)
					break;
				if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, &length))
					break;

				iv = encrs;
				cipher = encrs + SIZE_128_BITS;
				cipher_len = length - SIZE_128_BITS;
				if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &config, &config_len))
					break;

				target->config = config;
				target->config_len = config_len;

				fail = 0;
			} while (0);
			
			if (encrs)
				os_free(encrs);
			if (fail && config) {
				os_free(config);
				target->config = 0;
				target->config_len = 0;
			}
			if (fail)
				break;

			if (with_config)
				*with_config = 1;
		}

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M2(struct eap_sm *sm,
									  struct eap_wps_data *data,
									  Boolean *with_config)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M2(conf, data, with_config))
			break;

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_config_process_message_M2D(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M2D)
			break;

		if (eap_wps_config_process_message_M2_M2D(conf, data))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M2D(struct eap_sm *sm,
									   struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M2D(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 * eap_wps_config_build_message_M3(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 authenticator[SIZE_8_BYTES];
	u8 u8val;
	size_t length;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M3;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Generate Device Password, if it hasn't been set yet */
		if (!data->dev_pwd_len) {
                        #if 0   /* original from Sony */
			if (eap_wps_generate_device_password(data->dev_pwd, 8))
				break;
			data->dev_pwd_len = 8;
			data->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
			printf("eap_wps : generated device password [%s]\n", data->dev_pwd);
                        #else
                        /* Atheros:
                         * The GUI or other high level should generate a
                         * password... we should not do it here.
                         */
                        wpa_printf(MSG_INFO, "Reached M3 without password!");
                        break;
                        #endif
		}

		/* E-Hash1 */
		if (eap_wps_generate_hash(data, data->dev_pwd,
								   data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   data->pubKey, target->pubKey, data->authKey,
								   data->psk1, data->snonce1, data->hash1))
			break;
		if(wps_set_value(wps, WPS_TYPE_E_HASH1, data->hash1, sizeof(data->hash1)))
			break;

		/* E-Hash2 */
		if (eap_wps_generate_hash(data, data->dev_pwd + data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   data->dev_pwd_len/2,
								   data->pubKey, target->pubKey, data->authKey,
								   data->psk2, data->snonce2, data->hash2))
			break;
		if(wps_set_value(wps, WPS_TYPE_E_HASH2, data->hash2, sizeof(data->hash2)))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M3(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M3(conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M3(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 tmp[SIZE_64_BYTES];
	size_t length;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M3)
			break;

		/* Registrar Nonce */
		length = sizeof(tmp);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, tmp, &length))
			break;
		if (os_memcmp(tmp, data->nonce, sizeof(data->nonce)))
			break;

		/* E-Hash1 */
		length = sizeof(target->hash1);
		if (wps_get_value(wps, WPS_TYPE_E_HASH1, target->hash1, &length))
			break;

		/* E-Hash2 */
		length = sizeof(target->hash2);
		if (wps_get_value(wps, WPS_TYPE_E_HASH2, target->hash2, &length))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M3(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M3(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M4(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 authenticator[SIZE_8_BYTES];
	u8 u8val;
	size_t length;
	u8 *encrs;
	int encrs_len;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M4;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		if (!data->dev_pwd_len)
			break;

		/* R-Hash1 */
		if (eap_wps_generate_hash(data, data->dev_pwd,
								   data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   target->pubKey, data->pubKey, data->authKey,
								   data->psk1, data->snonce1, data->hash1))
			break;
		if(wps_set_value(wps, WPS_TYPE_R_HASH1, data->hash1, sizeof(data->hash1)))
			break;

		/* R-Hash2 */
		if (eap_wps_generate_hash(data, data->dev_pwd + data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   data->dev_pwd_len/2,
								   target->pubKey, data->pubKey, data->authKey,
								   data->psk2, data->snonce2, data->hash2))
			break;
		if(wps_set_value(wps, WPS_TYPE_R_HASH2, data->hash2, sizeof(data->hash2)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(data, WPS_TYPE_R_SNONCE1, data->snonce1, 0, 0, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;


		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M4(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M4(conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M4(struct wps_config *conf,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M4)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* R-Hash1 */
		length = sizeof(target->hash1);
		if (wps_get_value(wps, WPS_TYPE_R_HASH1, target->hash1, &length))
			break;

		/* R-Hash2 */
		length = sizeof(target->hash2);
		if (wps_get_value(wps, WPS_TYPE_R_HASH2, target->hash2, &length))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_R_SNONCE1, rsnonce, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* RHash1 validation */
		if (eap_wps_hash_validation(data, target->hash1, rsnonce, data->psk1, data->pubKey, target->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M4(struct eap_sm *sm,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M4(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M5(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	int encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M5;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(data, WPS_TYPE_E_SNONCE1, data->snonce1, 0, 0, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M5(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M5(conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M5(struct wps_config *conf,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	u8 version;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M5)
			break;

		/* Registrar Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_E_SNONCE1, rsnonce, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* EHash1 validation */
		if (eap_wps_hash_validation(data, target->hash1, rsnonce, data->psk1, target->pubKey, data->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M5(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M5(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M6(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	int encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M6;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(data, WPS_TYPE_R_SNONCE2, data->snonce2, 0, 0, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M6(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M6(conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M6(struct wps_config *conf,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M6)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_R_SNONCE2, rsnonce, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* RHash2 validation */
		if (eap_wps_hash_validation(data, target->hash2, rsnonce, data->psk2, data->pubKey, target->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M6(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M6(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M7(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	int encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M7;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(data, WPS_TYPE_E_SNONCE2, data->snonce2, 0, 0, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M7(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M7(conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M7(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M7)
			break;

		/* Registrar Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_E_SNONCE2, rsnonce, keyWrapAuth))
			break;
		if (target->config)
			os_free(target->config);
		target->config = decrypted;
		target->config_len = decrypted_len;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* EHash2 validation */
		if (eap_wps_hash_validation(data, target->hash2, rsnonce, data->psk2, target->pubKey, data->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (ret && decrypted) {
		os_free(decrypted);
		if (data->target) {
			target = data->target;
			target->config = 0;
			target->config_len = 0;
		}
	}

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M7(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M7(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M8(struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	int encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M8;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(data, 0, 0,
					data->config, data->config_len,
					data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M8(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M8(conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M8(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M8)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len,
											data->authKey, 0, 0, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		if (target->config)
			os_free(target->config);
		target->config = decrypted;
		target->config_len = decrypted_len;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (ret && decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M8(struct eap_sm *sm,
									 struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M8(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_special(struct wps_config *conf,
		struct eap_wps_data *data,
		u8 msg_type,
		u8 *e_nonce, u8 *r_nonce,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	size_t length;

	do {
		if (!conf || !data || !data->target || !e_nonce || !r_nonce || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, e_nonce, SIZE_UUID))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, r_nonce, SIZE_UUID))
			break;

		/* Configuration Error */
		if (WPS_MSGTYPE_NACK == msg_type) {
			if (wps_set_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
				break;
		}

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_special(struct eap_sm *sm,
										 struct eap_wps_data *data,
										 u8 msg_type,
										 u8 *e_nonce, u8 *r_nonce,
										 size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !e_nonce || !r_nonce || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_special(conf, data, msg_type,
			e_nonce, r_nonce, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_special(struct wps_config *conf,
		struct eap_wps_data *data,
		u8 msg_type,
		u8 *e_nonce, u8 *r_nonce)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 u8val;
	u8 nonce[SIZE_NONCE];
	size_t length;

	do {
		if (!conf || !data || !data->target || !e_nonce || !r_nonce)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;
		if (msg_type != u8val)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(e_nonce, nonce, length))
			break;

		/* Registrar Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, nonce, &length))
			break;
		if (os_memcmp(r_nonce, nonce, length))
			break;

		if (msg_type == WPS_MSGTYPE_NACK) {
			/* Configuration Error */
			if (wps_get_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
				break;
		}

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_special(struct eap_sm *sm,
										   struct eap_wps_data *data,
										   u8 msg_type,
										   u8 *e_nonce, u8 *r_nonce)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !e_nonce || !r_nonce || !conf)
			break;

		if (eap_wps_config_process_message_special(conf, data, msg_type,
												   e_nonce, r_nonce))
			break;
		ret = 0;
	} while (0);

	return ret;
}


static u8 *eap_wps_build_packet(u8 code, u8 identifier, u8 op_code, u8 flags,
		u8 *msg, size_t msg_len, size_t *rsp_len)
{
	u8 *rsp = 0;
	struct eap_hdr *rsp_hdr;
	struct eap_format *rsp_fmt;
	u8 *tmp;

	do {
		if ((!msg && msg_len) || !rsp_len)
			break;

		if (flags & EAP_FLAG_LF)
			*rsp_len = sizeof(*rsp_hdr) + sizeof(*rsp_fmt) + msg_len + 2;
		else
			*rsp_len = sizeof(*rsp_hdr) + sizeof(*rsp_fmt) + msg_len;
		rsp = wpa_zalloc(*rsp_len);
		
		if (rsp) {
			rsp_hdr = (struct eap_hdr *)rsp;
			rsp_hdr->code = code;
			rsp_hdr->identifier = identifier;
			rsp_hdr->length = host_to_be16(*rsp_len);

			rsp_fmt = (struct eap_format *)(rsp_hdr + 1);
			rsp_fmt->type = EAP_TYPE_EXPANDED;
			os_memcpy(rsp_fmt->vendor_id, EAP_VENDOR_ID_WPS, sizeof(rsp_fmt->vendor_id));
			os_memcpy(rsp_fmt->vendor_type, EAP_VENDOR_TYPE_WPS, sizeof(rsp_fmt->vendor_type));
			rsp_fmt->op_code = op_code;
			rsp_fmt->flags = flags;

			tmp = (u8 *)(rsp_fmt + 1);
			if (flags & EAP_FLAG_LF) {
				WPA_PUT_BE16(tmp, msg_len);
				tmp += 2;
			}

			if (msg_len)
				os_memcpy(tmp, msg, msg_len);
		}
	} while (0);

	if (!rsp && rsp_len)
		*rsp_len = 0;

	return rsp;
}


static u8 *eap_wps_process_registrar(struct eap_sm *sm,
									 struct eap_wps_data *data,
									 struct eap_method_ret *ret,
									 u8 req_identifier,
									 u8 req_op_code, size_t *rsp_len)
{
	u8 *rsp = 0;
	u8 *wps_msg = 0;
	size_t wps_msg_len;
	struct eap_wps_target_info *target = data->target;
        struct wpa_supplicant *wpa_s = sm->msg_ctx;

	do {
		if (data->mode != REGISTRAR)
			break;

		switch (req_op_code) {
		case EAP_OPCODE_WPS_MSG:
			if (WPS_MSGTYPE_M1 == wps_get_message_type(data->rcvMsg,
														   data->rcvMsgLen))
				data->state = START;
			break;
		case EAP_OPCODE_WPS_NACK:
			data->state = NACK;
			break;
		default:
			break;
		}

		switch (data->state) {
		case START:
		{
			/* Should be received M1 message */
			if (!eap_wps_process_message_M1(sm, data)) {
				if (data->dev_pwd_len) {
					/* Build M2 message */
					if (!(wps_msg = eap_wps_build_message_M2(sm, data, &wps_msg_len)))
						break;

					rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
												EAP_OPCODE_WPS_MSG, 0,
												wps_msg, wps_msg_len,
												rsp_len);
					if(!rsp) {
						ret->ignore = 1;
						break;
					}
					data->state = M2;
				} else {
					struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
					char msg[32];

					/* Build M2D message */
					if (!(wps_msg = eap_wps_build_message_M2D(sm, data, &wps_msg_len)))
						break;

					rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
												EAP_OPCODE_WPS_MSG, 0,
												wps_msg, wps_msg_len,
												rsp_len);
					if(!rsp) {
						ret->ignore = 1;
						break;
					}
					os_snprintf(msg, sizeof(msg), "REGISTRAR:%d", conf->nwid_trying_wps);
					eap_wps_request(sm, CTRL_REQ_TYPE_PASSWORD, msg, os_strlen(msg));
					data->state = M2D;
				}
			}
			break;
		}
		case M2:
		{
			/* Should be received M3 message */
			if (!eap_wps_process_message_M3(sm, data)) {
				/* Build M4 message */
				if (!(wps_msg = eap_wps_build_message_M4(sm, data, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_MSG, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;
				data->state = M4;
			}
			break;
		}
		case M2D:
		{
			/* Should be received NACK */
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce)) {
				/* Build NACK */
				if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_NACK, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;

				data->state = START;
			}
			break;
		}
		case M4:
		{
			/* Should be received M5 message */
			if (!eap_wps_process_message_M5(sm, data)) {
				/* Build M6 message */
				if (!(wps_msg = eap_wps_build_message_M6(sm, data, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_MSG, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;
				data->state = M6;
			}
			break;
		}
		case M6:
		{
			/* Should be received M7 message */
			if (!eap_wps_process_message_M7(sm, data)) {
				/* Build M8 message */
				if (!(wps_msg = eap_wps_build_message_M8(sm, data, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_MSG, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;
				data->state = M8;
			}
			break;
		}
		case M8:
		{
			struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

			/* Should be received Done */
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_DONE, target->nonce, data->nonce)) {
				/* Build ACK */
				if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_ACK, target->nonce, data->nonce, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_ACK, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;

				switch (data->reg_mode) {
				case WPS_SUPPLICANT_REGMODE_CONFIGURE_AP:
					/* Select Network Configuration (already added) */
					(void)eap_wps_select_ssid_configuration(sm, data, data->config, data->config_len, 0);
					break;
				case WPS_SUPPLICANT_REGMODE_REGISTER_AP:
					/* Set Network Configuration */
					(void)eap_wps_set_supplicant_ssid_configuration(sm, data, target->config, target->config_len, 0);
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

				data->state = START;
                                /* All done (?). Disable WPS mode now,
                                 * including killing off the temporary network
                                 * description we were using.
                                 * The original Sony code did not ever
                                 * seem to disable WPS when done... !
                                 */
                                (void) eap_wps_done_delayed(wpa_s, conf);
			}
			break;
		}
		case NACK:
		{
			struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
			char msg[32];

			/* Should be received NACK */
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce)) {
				/* Build NACK */
				if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_NACK, 0,
											wps_msg, wps_msg_len,
											rsp_len);

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

				os_snprintf(msg, sizeof(msg), "%d", conf->nwid_trying_wps);
				eap_wps_request(sm, CTRL_REQ_TYPE_FAIL, msg, os_strlen(msg));
		                eap_wps_request(sm, CTRL_REQ_TYPE_DONE, 0, 0);

				data->state = START;
			}

			break;
		}
		default:
		{
			break;
		}
		}
	} while (0);

	if (wps_msg) {
		os_free(wps_msg);
		wps_msg = 0;
	}

	if (!rsp && rsp_len)
		*rsp_len = 0;

	if (!ret->ignore && !rsp) {
		struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
		char msg[32];

		do {
			/* Build NACK */
			if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce, &wps_msg_len)))
				break;

			rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
										EAP_OPCODE_WPS_NACK, 0,
										wps_msg, wps_msg_len,
										rsp_len);
		} while (0);

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

		os_snprintf(msg, sizeof(msg), "%d", conf->nwid_trying_wps);
		eap_wps_request(sm, CTRL_REQ_TYPE_FAIL, msg, os_strlen(msg));
		eap_wps_request(sm, CTRL_REQ_TYPE_DONE, 0, 0);

		data->state = START;
                /* All done. Disable WPS mode now,
                * including killing off the temporary network
                * description we were using.
                * The original Sony code did not ever
                * seem to disable WPS when done... !
                */
                (void) eap_wps_done_delayed(wpa_s, conf);

		if (wps_msg) {
			os_free(wps_msg);
			wps_msg = 0;
		}

		if (!rsp) {
			ret->ignore = 1;
			if (rsp_len)
				*rsp_len = 0;
		}
	}

	return rsp;
}


static u8 *eap_wps_process_enrollee(struct eap_sm *sm,
									struct eap_wps_data *data,
									struct eap_method_ret *ret,
									u8 req_identifier,
									u8 req_op_code, size_t *rsp_len)
{
	u8 *rsp = 0;
	u8 *wps_msg = 0;
	size_t wps_msg_len;
	struct eap_wps_target_info *target = data->target;
        struct wpa_supplicant *wpa_s = sm->msg_ctx;

        wpa_printf(MSG_INFO, "eap_wps_process_enrollee: Enter in state %d\n",
                data->state);

	do {
		if (data->mode != ENROLLEE)
			break;

		switch (req_op_code) {
		case EAP_OPCODE_WPS_START:
                        wpa_printf(MSG_INFO, 
                                "eap_wps_process_enrollee: to START\n");
			data->state = START;
			break;
		case EAP_OPCODE_WPS_MSG:
			break;
		case EAP_OPCODE_WPS_NACK:
                        wpa_printf(MSG_INFO, 
                                "eap_wps_process_enrollee: to NACK\n");
			data->state = NACK;
			break;
		default:
			break;
		}

		switch (data->state) {
		case START:
		{
			/* Should be received Start Message */
			/* Build M1 message */
                        wpa_printf(MSG_INFO, 
                                "eap_wps_process_enrollee: build M1\n");
			if (!(wps_msg = eap_wps_build_message_M1(sm, data, &wps_msg_len)))
				break;

			rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										rsp_len);
			if(!rsp)
				break;
			data->state = M1;
			break;
		}
		case M1:
		case ACK:
		{
			Boolean with_config;
			u8 op_code;
			int next;

			/* Should be received M2/M2D message */
                        wpa_printf(MSG_INFO, 
                                "eap_wps_process_enrollee: process M2[D]\n");
			if (!eap_wps_process_message_M2(sm, data, &with_config)) {
				/* Received M2 */
				if (with_config) {
					/* Build Done */
					if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_DONE, data->nonce, target->nonce, &wps_msg_len)))
						break;

					/* Set Network Configuration */
					(void)eap_wps_set_supplicant_ssid_configuration(sm, data, target->config, target->config_len, 1);

					op_code = EAP_OPCODE_WPS_DONE;
                                        wpa_printf(MSG_INFO, 
                                                "eap_wps_process_enrollee: Done at M2\n");
					next = START;
				} else {
					/* Build M3 message */
                                        wpa_printf(MSG_INFO, 
                                                "eap_wps_process_enrollee: Build M3\n");
					if (!(wps_msg = eap_wps_build_message_M3(sm, data, &wps_msg_len)))
						break;
					op_code = EAP_OPCODE_WPS_MSG;
					next = M3;
				}
				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											op_code, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;
				data->state = next;
			} else if (!eap_wps_process_message_M2D(sm, data)) {
				struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
				char msg[32];
				int len;

				/* Received M2D */
				/* Build ACK */
                                wpa_printf(MSG_INFO, 
                                        "eap_wps_process_enrollee: got M2D, build ACK\n");
				if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_ACK, data->nonce, target->nonce, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_ACK, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;

				if (ACK != data->state) {
					len = os_snprintf(msg, sizeof(msg), "ENROLLEE:%d", conf->nwid_trying_wps);
					if ((data->dev_pwd_id == WPS_DEVICEPWDID_DEFAULT) &&
						data->dev_pwd_len) {
						len += os_snprintf(msg + len, sizeof(msg) - len, "-");
						strncpy(msg + len, (char *)data->dev_pwd, 8);
						len += 8;
						msg[len] = 0;
					}

					eap_wps_request(sm, CTRL_REQ_TYPE_PASSWORD, msg, os_strlen(msg));
					data->state = ACK;
				}
			}
			break;
		}
		case M3:
		{
			/* Should be received M4 message */
                        wpa_printf(MSG_INFO, 
                                "eap_wps_process_enrollee: process M4\n");
			if (!eap_wps_process_message_M4(sm, data)) {
				/* Build M5 message */
                                wpa_printf(MSG_INFO, 
                                        "eap_wps_process_enrollee: build M5\n");
				if (!(wps_msg = eap_wps_build_message_M5(sm, data, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_MSG, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;
				data->state = M5;
			}
			break;
		}
		case M5:
		{
			/* Should be received M6 message */
                        wpa_printf(MSG_INFO, 
                                "eap_wps_process_enrollee: process M6\n");
			if (!eap_wps_process_message_M6(sm, data)) {
				/* Build M7 message */
                                wpa_printf(MSG_INFO, 
                                        "eap_wps_process_enrollee: to M7\n");
				if (!(wps_msg = eap_wps_build_message_M7(sm, data, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_MSG, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;
				data->state = M7;
			}
			break;
		}
		case M7:
		{
			struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

			/* Should be received M8 message */
                        wpa_printf(MSG_INFO, 
                                "eap_wps_process_enrollee: process M8\n");
			if (!eap_wps_process_message_M8(sm, data)) {
				/* Build Done */
                                wpa_printf(MSG_INFO, 
                                        "eap_wps_process_enrollee: build DONE\n");
				if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_DONE, data->nonce, target->nonce, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_DONE, 0,
											wps_msg, wps_msg_len,
											rsp_len);
				if(!rsp)
					break;

				/* Set Network Configuration */
				(void)eap_wps_set_supplicant_ssid_configuration(sm, data, target->config, target->config_len, 1);

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

				data->state = START;
                                /* All done. Disable WPS mode now,
                                 * including killing off the temporary network
                                 * description we were using.
                                 * The original Sony code did not ever
                                 * seem to disable WPS when done... !
                                 */
                                (void) eap_wps_done_delayed(wpa_s, conf);
			}
			break;
		}
		case NACK:
		{
			struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
			char msg[32];

			/* Should be received NACK message */
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_NACK, data->nonce, target->nonce)) {
				/* Build NACK */
				if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, data->nonce, target->nonce, &wps_msg_len)))
					break;

				rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
											EAP_OPCODE_WPS_NACK, 0,
											wps_msg, wps_msg_len,
											rsp_len);

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

				os_snprintf(msg, sizeof(msg), "%d", conf->nwid_trying_wps);
				eap_wps_request(sm, CTRL_REQ_TYPE_FAIL, msg, os_strlen(msg));
		                eap_wps_request(sm, CTRL_REQ_TYPE_DONE, 0, 0);

				data->state = START;
                                /* All done. Disable WPS mode now,
                                 * including killing off the temporary network
                                 * description we were using.
                                 * The original Sony code did not ever
                                 * seem to disable WPS when done... !
                                 */
                                (void) eap_wps_done_delayed(wpa_s, conf);
			}
			break;
		}
		default:
		{
			break;
		}
		}
	} while (0);

	if (wps_msg) {
		os_free(wps_msg);
		wps_msg = 0;
	}

	if (!rsp && rsp_len)
		*rsp_len = 0;

	if (!ret->ignore && !rsp) {
		struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
		char msg[32];

		do {
			/* Build NACK */
			if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, data->nonce, target->nonce, &wps_msg_len)))
				break;

			rsp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_identifier,
										EAP_OPCODE_WPS_NACK, 0,
										wps_msg, wps_msg_len,
										rsp_len);
		} while (0);

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

		os_snprintf(msg, sizeof(msg), "%d", conf->nwid_trying_wps);
		eap_wps_request(sm, CTRL_REQ_TYPE_FAIL, msg, os_strlen(msg));
		eap_wps_request(sm, CTRL_REQ_TYPE_DONE, 0, 0);

		data->state = START;
                /* All done. Disable WPS mode now,
                * including killing off the temporary network
                * description we were using.
                * The original Sony code did not ever
                * seem to disable WPS when done... !
                */
                (void) eap_wps_done_delayed(wpa_s, conf);

		if (wps_msg) {
			os_free(wps_msg);
			wps_msg = 0;
		}

		if (!rsp) {
			ret->ignore = 1;
			if (rsp_len)
				*rsp_len = 0;
		}
	}

	return rsp;
}


static u8 *eap_wps_process(struct eap_sm *sm, void *priv,
						   struct eap_method_ret *ret,
						   const u8 *reqData, size_t reqDataLen,
						   size_t *respDataLen)
{
	u8 *resp = 0;
	struct eap_wps_data *data = (struct eap_wps_data *)priv;
	struct eap_hdr *req_hdr = (struct eap_hdr *)reqData;
	struct eap_format *req_fmt;
	const u8 *identity;
	size_t identity_len;
	u8 *raw;
	u16 msg_len;

	do {
		ret->ignore = 0;

		req_fmt = (struct eap_format *)(req_hdr + 1);
		if (be_to_host16(req_hdr->length) != reqDataLen) {
			ret->ignore = 1;
			break;
		} else if ((EAP_TYPE_EXPANDED != req_fmt->type) ||
				   (0 != os_memcmp(req_fmt->vendor_id, EAP_VENDOR_ID_WPS,
				                sizeof(req_fmt->vendor_id))) ||
				   (0 != os_memcmp(req_fmt->vendor_type, EAP_VENDOR_TYPE_WPS,
				                sizeof(req_fmt->vendor_type)))) {
			ret->ignore = 1;
			break;
		}

		if (req_fmt->flags & EAP_FLAG_LF) {
			raw = (u8 *)(req_fmt + 1);
			msg_len = req_hdr->length - (sizeof(*req_hdr) + sizeof(*req_fmt));
			if (msg_len != WPA_GET_BE16((u8 *)req_fmt + 1)) {
				ret->ignore = 1;
				return 0;
			}
		} else {
			raw = (u8 *)(req_fmt + 1);
			msg_len = reqDataLen - (sizeof(*req_hdr) + sizeof(*req_fmt));
		}

		if (data->fragment) {
			data->fragment = 0;
			data->rcvMsg = (u8 *)os_realloc(data->rcvMsg, data->rcvMsgLen + msg_len);
			if (data->rcvMsg) {
				os_memcpy(data->rcvMsg + data->rcvMsgLen, raw, msg_len);
				data->rcvMsgLen += msg_len;
			}
		} else {
			if (data->rcvMsg)
				os_free(data->rcvMsg);
			data->rcvMsg = (u8 *)os_malloc(msg_len);
			if (data->rcvMsg) {
				os_memcpy(data->rcvMsg, raw, msg_len);
				data->rcvMsgLen = msg_len;
			}
		}

		if (!data->rcvMsg) {
			/* Memory allocation Error */
			data->rcvMsgLen = 0;
			break;
		}

		if (req_fmt->flags & EAP_FLAG_MF) {
			data->fragment = 1;
			resp = eap_wps_build_packet(EAP_CODE_RESPONSE, req_hdr->identifier,
										EAP_OPCODE_WPS_FLAG_ACK, 0, NULL, 0,
										respDataLen);
			if (resp)
				data->fragment = 1;
			break;
		}

		identity = eap_get_config_identity(sm, &identity_len);
		if (0 == os_strcmp((char *)identity, WPS_IDENTITY_REGISTRAR))
			data->mode = REGISTRAR;
		else if (0 == os_strcmp((char *)identity, WPS_IDENTITY_ENROLLEE))
			data->mode = ENROLLEE;
		else {
			/* Error */
			ret->ignore = 1;
			return 0;
		}

		switch (data->mode) {
		case REGISTRAR:
			resp = eap_wps_process_registrar(sm, data, ret,
											 req_hdr->identifier,
											 req_fmt->op_code,
											 respDataLen);
			break;
		case ENROLLEE:
			resp = eap_wps_process_enrollee(sm, data, ret,
											req_hdr->identifier,
											req_fmt->op_code,
											respDataLen);
			break;
		default:
			break;
		}
	} while (0);

	return resp;
}

int eap_peer_wps_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_peer_method_alloc(EAP_PEER_METHOD_INTERFACE_VERSION,
				    WPA_GET_BE24(EAP_VENDOR_ID_WPS), WPA_GET_BE32(EAP_VENDOR_TYPE_WPS), "WPS");
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


/* Caution: this depends on wps->wps_job_busy being correct, as well
 * as other parameters.
 */
static int eap_wps_set_ie(struct wpa_supplicant *wpa_s, struct wps_config *wps)
{
	int ret = -1;
	u8 *iebuf = 0;
	size_t iebuflen;

	do {
		if (!wpa_s || !wps)
			break;
                if (wps->wps_disable) 
                        break;

		if (wps->is_push_button) {
                        #if 0   /* was from Sony code */
			os_memset(wps->pub_key, 0, sizeof(wps->pub_key));
			wps_config_free_dh(&wps->dh_secret);
			wps->set_pub_key = 0;
                        #endif

			/* Create WPS ProbeReq IE */
			if (wps_config_create_probe_req_ie(wpa_s, &iebuf, &iebuflen)) {
				break;
			}
			/* Set WPS ProbeReq IE */
			if (wpa_drv_set_wps_probe_req_ie(wpa_s, iebuf, iebuflen)) {
				break;
			}
			free(iebuf);
			iebuf = 0;
			iebuflen = 0;
			/* Create WPS AssocReq IE */
			if (wps_config_create_assoc_req_ie(wpa_s, &iebuf, &iebuflen)) {
				break;
			}
			/* Set WPS AssocReq IE */
			if (wpa_drv_set_wps_assoc_req_ie(wpa_s, iebuf, iebuflen)) {
				break;
			}
		} else {
                        #if 0   /* was from sony code */
			wps->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
			os_memset(wps->dev_pwd, 0, sizeof(wps->dev_pwd));
			wps->dev_pwd_len = 0;
                        #endif

			/* Clear WPS ProbeReq IE */
			if (wpa_drv_set_wps_probe_req_ie(wpa_s, 0, 0)) {
				break;
			}
			/* Set WPS AssocReq IE */
			if (wpa_drv_set_wps_assoc_req_ie(wpa_s, 0, 0)) {
				break;
			}
		}

		ret = 0;
	} while (0);
	if (iebuf)
		os_free(iebuf);

	return ret;
}


static void eap_wps_timer_tick(void *ctx, void *conf)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wps_config *wps = conf;
	struct os_time now;
	int timeout = 0;

	if(!wps->wps_job_busy) {
		os_memset(&wps->end_time, 0, sizeof(wps->end_time));
		return;
	}

	os_get_time(&now);
	if (now.sec > wps->end_time.sec)
		timeout = 1;
	else if ((now.sec == wps->end_time.sec) &&
			 (now.usec >= wps->end_time.usec))
		timeout = 1;

	if (timeout) {
                if (wps->wps_done) {
                        wpa_printf(MSG_ERROR, "WPS DONE");
		        wpa_msg(wpa_s, MSG_INFO, "WPS DONE");
                } else {
                        wpa_printf(MSG_ERROR, "WPS timeout");
		        wpa_msg(wpa_s, MSG_INFO, "WPS timeout");
                }
                (void) eap_wps_disable(wpa_s, wps);
	} else {
		eloop_register_timeout(EAP_WPS_PERIOD_SEC, EAP_WPS_PERIOD_USEC,
                        eap_wps_timer_tick, ctx, conf);
        }
}


int eap_wps_enable(struct wpa_supplicant *wpa_s, struct wps_config *wps,
        struct eap_wps_enable_params *params)
{
        u8 *dev_pwd = params->dev_pwd;    /* 00000000 for push button method */
        int dev_pwd_len = params->dev_pwd_len;
        int filter_bssid_flag = params->filter_bssid_flag;   /* accept only given bssid? */
        u8  *filter_bssid = params->filter_bssid;     /* used if filter_bssid_flag */
        int filter_ssid_length = params->filter_ssid_length;  /* accept only given essid? */
        u8  *filter_ssid = params->filter_ssid;
        int was_enabled = wps->wps_job_busy;
	int ret = -1;
        int is_push_button = 
                (dev_pwd_len == 8 && memcmp(dev_pwd, "00000000", 8) == 0);

        if (dev_pwd_len == 0) {
                /* Default to push button method */
                dev_pwd = (u8 *)"00000000";
                dev_pwd_len = 8;
                is_push_button = 1;
        }

        wpa_printf(MSG_INFO, "eap_wps_enable pwd=%.*s",
                dev_pwd_len, dev_pwd);

        /* This is a confusing situation, because the password
         * might have changed... but it might just be the user
         * adding more time to the timer or ?
         * hopefully it is sufficient
         * just to start anew without officially cancelling
         * the old one.
         * (In any case, don't send a "stop" message unless we're really
         * stopping!).
         */
    	eloop_cancel_timeout(eap_wps_timer_tick, wpa_s, wps);

	do {
                if (wps->wps_disable) {
                        wpa_printf(MSG_INFO, "WPS is disabled for this BSS");
                        break;
                }
	        wps->wps_job_busy = 1;   /* make sure we clean up after this */
                wps->is_push_button = is_push_button;

		(void)os_get_time(&wps->end_time);
		wps->end_time.sec += EAP_WPS_TIMEOUT_SEC;
		wps->end_time.usec += EAP_WPS_TIMEOUT_USEC;

                os_memset(wps->dev_pwd, 0, sizeof(wps->dev_pwd));
                wps->dev_pwd_len = dev_pwd_len;
                os_memcpy(wps->dev_pwd, dev_pwd, wps->dev_pwd_len);

                if (wps->is_push_button) {
			wps->dev_pwd_id = WPS_DEVICEPWDID_PUSH_BTN;
                } else {
                        /* WPS allows various lengths, but length 8
                        *  is special... must be numeric and last digit
                        *  must be checksum of first 7.
                        */
                        if ((8 == dev_pwd_len) && 
                                    eap_wps_device_password_validation(
                                        (u8 *)dev_pwd, dev_pwd_len)) {
                                wpa_printf(MSG_ERROR,
                                    "wps_enable: Bad checksum on PIN");
		                wpa_msg(wpa_s, MSG_ERROR, 
                                        "WPS Error: Invalid PIN (bad checksum)");
                                break;
                        }
			wps->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT /*Use PIN*/;
                }

                if (filter_bssid_flag) {
                        wps->filter_bssid_flag = 1;
                        memcpy(wps->filter_bssid, filter_bssid,
                                sizeof(wps->filter_bssid));
                }
                if (filter_ssid_length > 0) {
                        if (filter_ssid_length > 32) {
                            break;      /* invalid */
                        }
                        wps->filter_ssid_length = filter_ssid_length;
                        memset(wps->filter_ssid, 0, sizeof(wps->filter_ssid));
                        memcpy(wps->filter_ssid, filter_ssid,
                                filter_ssid_length);
                }
       	        if (eap_wps_set_ie(wpa_s, wps)) {
				break;
                }

		if (!was_enabled) {
			wps->nwid_trying_wps = -1;
			wpa_msg(wpa_s, MSG_INFO, "WPS start");
		} else {
			wpa_msg(wpa_s, MSG_INFO, "WPS restart");
                }
		eloop_register_timeout(EAP_WPS_PERIOD_SEC, EAP_WPS_PERIOD_USEC,
                        eap_wps_timer_tick, wpa_s, wps);

                /* Re-scan and re-associate */
		if (wpa_s->current_ssid) wpa_supplicant_disassociate(
                        wpa_s, REASON_DEAUTH_LEAVING);
		if (wpa_s->disconnected)
			wpa_s->disconnected = 0;
		wpa_s->scan_req = 2;    /* force scan */
		wpa_supplicant_req_scan(wpa_s, 0, 0);

		ret = 0;
	} while (0);

        if (ret) {
                wpa_printf(MSG_ERROR, "Failed to start WPS -- disabling.");
                eap_wps_disable(wpa_s, wps);
        }

	return ret;
}

int eap_wps_disable(struct wpa_supplicant *wpa_s, struct wps_config *wps)
{
	int ret = -1;
	struct wpa_ssid *tmp;

	do {
		os_memset(&wps->end_time, 0, sizeof(wps->end_time));

		if (-1 != wps->nwid_trying_wps) {
			(void)wps_config_remove_network(
                                wpa_s, wps->nwid_trying_wps);
			wps->nwid_trying_wps = -1;
		}

		if (wps->wps_job_busy) {
			eloop_cancel_timeout(eap_wps_timer_tick, wpa_s, wps);
		        wps->wps_job_busy = 0;
                        wps->is_push_button = 0;
                        wps->wps_done = 0;
		        wps->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
			os_memset(wps->pub_key, 0, sizeof(wps->pub_key));
			wps_config_free_dh(&wps->dh_secret);
			wps->set_pub_key = 0;

                        wps->filter_bssid_flag = 0;
                        memset(wps->filter_bssid, 0, sizeof(wps->filter_bssid));
                        wps->filter_ssid_length = 0;
                        memset(wps->filter_ssid, 0, sizeof(wps->filter_ssid));
                        wps->do_save = 0;

		        tmp = wpa_s->conf->ssid;
		        while (tmp) {
                                /* Clear bit 1 of disabled -- "temporary WPS bit" */
			        tmp->disabled &= ~0x2;
			        tmp = tmp->next;
		        }

		        wpa_s->current_ssid = NULL;
		        wpa_s->disconnected = 1;
		        wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
			if (eap_wps_set_ie(wpa_s, wps))
				break;

			wpa_msg(wpa_s, MSG_INFO, "WPS stop");

                        /* Reassociate ... or do we need to wait to do this? */
                        /* %%%%%%%% ? */
		        wpa_s->disconnected = 0;
		        wpa_s->reassociate = 1;
		        wpa_s->scan_req = 2;    /* force scan */
		        wpa_supplicant_req_scan(wpa_s, 0, 0);
		}

		ret = 0;
	} while (0);

	return ret;
}

/*
 * Kill off a finished WPS after a small delay 
 * (to allow ack packets to get through).
 */
int eap_wps_done_delayed(struct wpa_supplicant *wpa_s, struct wps_config *wps)
{
	int ret = -1;

	do {
		if (wps->wps_job_busy) {
		        (void)os_get_time(&wps->end_time);
		        wps->end_time.sec += EAP_WPS_CLEANUP_TIMEOUT_SEC;
		        wps->end_time.usec += EAP_WPS_CLEANUP_TIMEOUT_USEC;
                        wps->wps_done = 1;
		}
		ret = 0;
	} while (0);

	return ret;
}



