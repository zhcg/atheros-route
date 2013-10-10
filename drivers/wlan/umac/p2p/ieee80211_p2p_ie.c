#include "ieee80211_var.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211P2P_api.h"
#include "ieee80211_ie_utils.h"
#include "ieee80211_p2p_ie.h"

#if UMAC_SUPPORT_P2P

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif


u_int8_t *ieee80211_p2p_add_p2p_ie(u_int8_t *frm)
{
    u_int8_t  wfa_oui[3] = IEEE80211_P2P_WFA_OUI;
    struct ieee80211_p2p_ie *p2pie=(struct ieee80211_p2p_ie *) frm;

    p2pie->p2p_id = IEEE80211_P2P_IE_ID;
    p2pie->p2p_oui[0] = wfa_oui[0];
    p2pie->p2p_oui[1] = wfa_oui[1];
    p2pie->p2p_oui[2] = wfa_oui[2];
    p2pie->p2p_oui_type = IEEE80211_P2P_WFA_VER;
    p2pie->p2p_len=4;
    return (frm + sizeof(struct ieee80211_p2p_ie));
}

/*
 * Add the NOA IE to the frame
 * The tsf_offset parameter is used to skew the start times.
 */
u_int8_t *ieee80211_p2p_add_noa(
    u_int8_t *frm, 
    struct ieee80211_p2p_sub_element_noa *noa, 
    u_int32_t tsf_offset)
{
    u_int8_t    tmp_octet;
    int         i;

    *frm++ = IEEE80211_P2P_SUB_ELEMENT_NOA;     /* sub-element id */
    ASSERT(noa->num_descriptors <= IEEE80211_MAX_NOA_DESCRIPTORS);

    /*
     * Length = (2 octets for Index and CTWin/Opp PS) and
     * (13 octets for each NOA Descriptors)
     */
    P2PIE_PUT_LE16(frm, IEEE80211_NOA_IE_SIZE(noa->num_descriptors));
    frm++;
    frm++;

    *frm++ = noa->index;        /* Instance Index */

    tmp_octet = noa->ctwindow & IEEE80211_P2P_NOE_IE_CTWIN_MASK;
    if (noa->oppPS) {
        tmp_octet |= IEEE80211_P2P_NOE_IE_OPP_PS_SET;
    }
    *frm++ = tmp_octet;         /* Opp Ps and CTWin capabilities */

    for (i = 0; i < noa->num_descriptors; i++) {
        ASSERT(noa->noa_descriptors[i].type_count != 0);

        *frm++ = noa->noa_descriptors[i].type_count;
        
        P2PIE_PUT_LE32(frm, noa->noa_descriptors[i].duration);
        frm += 4;
        P2PIE_PUT_LE32(frm, noa->noa_descriptors[i].interval);
        frm += 4;
        P2PIE_PUT_LE32(frm, noa->noa_descriptors[i].start_time + tsf_offset);
        frm += 4;          
    }

    return frm;
}

int ieee80211_p2p_parse_noa(const u_int8_t *frm, u_int8_t num_noa_descriptors, struct ieee80211_p2p_sub_element_noa *noa)
{
    u_int8_t i;

    noa->index = *frm;
    ++frm;

    noa->oppPS = ((*frm) & (u_int8_t)IEEE80211_P2P_NOE_IE_OPP_PS_SET) ? 1 : 0;
    noa->ctwindow = ((*frm) & IEEE80211_P2P_NOE_IE_CTWIN_MASK);
    ++frm;

    noa->num_descriptors = num_noa_descriptors;

    for (i=0;i<num_noa_descriptors;++i) {
        noa->noa_descriptors[i].type_count = *frm;
        ++frm;
        noa->noa_descriptors[i].duration = le32toh(*((u_int32_t*)frm));
        frm += 4;
        noa->noa_descriptors[i].interval = le32toh(*((u_int32_t*)frm));
        frm += 4;
        noa->noa_descriptors[i].start_time = le32toh(*((u_int32_t*)frm));
        frm += 4;
    }
    return EOK;
}

int ieee80211_p2p_parse_group_info(const u8 *gi, size_t gi_len,
			 struct p2p_group_info *info)
{
	const u8 *g, *gend;

	OS_MEMSET(info, 0, sizeof(*info));
	if (gi == NULL)
		return 0;

	g = gi;
	gend = gi + gi_len;
	while (g < gend) {
		struct p2p_client_info *cli;
		const u8 *t, *cend;
		int count;

		cli = &info->client[info->num_clients];
		cend = g + 1 + g[0];
		if (cend > gend)
			return -1; /* invalid data */
		/* g at start of P2P Client Info Descriptor */
		/* t at Device Capability Bitmap */
		t = g + 1 + 2 * ETH_ALEN;
		if (t > cend)
			return -1; /* invalid data */
		cli->p2p_device_addr = g + 1;
		cli->p2p_interface_addr = g + 1 + ETH_ALEN;
		cli->dev_capab = t[0];

		if (t + 1 + 2 + 8 + 1 > cend)
			return -1; /* invalid data */

		cli->config_methods = P2PIE_GET_BE16(&t[1]);
		cli->pri_dev_type = &t[3];

		t += 1 + 2 + 8;
		/* t at Number of Secondary Device Types */
		cli->num_sec_dev_types = *t++;
		if (t + 8 * cli->num_sec_dev_types > cend)
			return -1; /* invalid data */
		cli->sec_dev_types = t;
		t += 8 * cli->num_sec_dev_types;

		/* t at Device Name in WPS TLV format */
		if (t + 2 + 2 > cend)
			return -1; /* invalid data */
		if (P2PIE_GET_BE16(t) != ATTR_DEV_NAME)
			return -1; /* invalid Device Name TLV */
		t += 2;
		count = P2PIE_GET_BE16(t);
		t += 2;
		if (count > cend - t)
			return -1; /* invalid Device Name TLV */
		if (count >= 32)
			count = 32;
		cli->dev_name = (const char *) t;
		cli->dev_name_len = count;

		g = cend;

		info->num_clients++;
		if (info->num_clients == P2P_MAX_GROUP_ENTRIES)
			return -1;
	}

	return 0;
}

static inline struct iebuf *
ieee802_11_vendor_ie_concat(osdev_t oshandle, const u8 *ies, size_t ies_len, const u8 *wfa_oui, u8 oui_type)
{
	struct iebuf *buf;
	const u8 *end, *pos, *ie;

    if ((ies == NULL) || (ies_len < 2))
        return NULL;
	pos = ies;
	end = ies + ies_len;
	ie = NULL;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			return NULL;
		if (pos[0] == IEEE80211_P2P_SUB_ELEMENT_VENDOR &&
            pos[1] >= 4 &&
            pos[2] == wfa_oui[0] &&
            pos[3] == wfa_oui[1] &&
            pos[4] == wfa_oui[2] &&
            pos[5] == oui_type)
        {
			ie = pos;
			break;
		}
		pos += 2 + pos[1];
	}

	if (ie == NULL)
		return NULL; /* No specified vendor IE found */

	buf = iebuf_alloc(oshandle, ies_len);
	if (buf == NULL)
		return NULL;

	/*
	 * There may be multiple vendor IEs in the message, so need to
	 * concatenate their data fields.
	 */
	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == IEEE80211_P2P_SUB_ELEMENT_VENDOR &&
            pos[1] >= 4 &&
            pos[2] == wfa_oui[0] &&
            pos[3] == wfa_oui[1] &&
            pos[4] == wfa_oui[2] &&
            pos[5] == oui_type)
        {
			iebuf_put_data(buf, pos + 6, pos[1] - 4);
        }
		pos += 2 + pos[1];
	}

	return buf;
}

static int
ieee80211_p2p_parse_subelement(wlan_if_t vap, u8 id, const u8 *data, u16 len,
				struct p2p_parsed_ie *msg)
{
	const u8 *pos;
	size_t i, nlen;

	switch (id) {
	case IEEE80211_P2P_SUB_ELEMENT_STATUS:
		if (len < 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Status "
				   "subelement (length %d)\n", __func__, len);
			return -1;
		}
		msg->status = data;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Status: %d\n", __func__, data[0]);
		break;
    /* case IEEE80211_P2P_SUB_ELEMENT_MINOR_REASON: */
	case IEEE80211_P2P_SUB_ELEMENT_CAPABILITY:
		if (len < 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Capability "
				   "subelement (length %d)\n", __func__, len);
			return -1;
		}
		msg->capability = data;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Device Capability %02x "
			   "Group Capability %02x\n", __func__, data[0], data[1]);
		break;
    case IEEE80211_P2P_SUB_ELEMENT_DEVICE_ID:
        if (len < ETH_ALEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Device ID "
				   "attribute (length %d)", __func__, len);
			return -1;
		}
		msg->device_id = data;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Device ID " MACSTR,
			   __func__, MAC2STR(msg->device_id));
		break;
	case IEEE80211_P2P_SUB_ELEMENT_GO_INTENT:
		if (len < 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short GO Intent "
				   "subelement length %d\n", __func__, len);
			return -1;
		}
		msg->go_intent = data;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * GO Intent: Intent %u "
			   "Tie breaker %u\n", __func__, data[0] >> 1, data[0] & 0x01);
		break;
	case IEEE80211_P2P_SUB_ELEMENT_CONFIGURATION_TIMEOUT:
		if (len < 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Configuration "
				   "Timeout subelement (length %d)\n", __func__, len);
			return -1;
		}
		msg->config_timeout = data;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Configuration Timeout\n",
                          __func__);
		break;
	case IEEE80211_P2P_SUB_ELEMENT_CHANNEL:
		if (len == 0) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Channel: Ignore null "
                              "channel (len %d)\n", __func__, len);
			break;
		}
		if (len < 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Channel "
				   "subelement (length %d)\n", __func__, len);
			return -1;
		}
		msg->channel = data;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Channel: Regulatory Class %d "
			   "Channel Number %d\n", __func__, data[0], data[1]);
		break;
    /* case IEEE80211_P2P_SUB_ELEMENT_GROUP_BSSID: */
    /* case IEEE80211_P2P_SUB_ELEMENT_EXTENDED_LISTEN_TIMING: */
	case IEEE80211_P2P_SUB_ELEMENT_INTENDED_INTERFACE_ADDR:
		if (len < IEEE80211_ADDR_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Intended P2P "
				   "Interface Address subelement (length %d)\n", __func__,
				   len);
			return -1;
		}
		msg->intended_addr = data;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Intended P2P Interface Address: "
			              MACSTR, __func__,  MAC2STR(msg->intended_addr));
		break;
    /* case IEEE80211_P2P_SUB_ELEMENT_MANAGEABILITY: */
	case IEEE80211_P2P_SUB_ELEMENT_CHANNEL_LIST:
		if (len < 3) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Channel List "
				   "subelement (length %d)\n", __func__, len);
			return -1;
		}
		msg->channel_list = data;
		msg->channel_list_len = len;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Channel List: Country String "
			   "'%c%c%c'\n", __func__, data[0], data[1], data[2]);
		break;
    case IEEE80211_P2P_SUB_ELEMENT_NOA:
        if ((len < 2) || ((len - 2) % IEEE80211_P2P_NOA_DESCRIPTOR_LEN) != 0 )  {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid NOA "
				   "subelement size (length %d)\n", __func__, len);
            return -1;
        }
        len = (len - 2)/IEEE80211_P2P_NOA_DESCRIPTOR_LEN;
        if (len > IEEE80211_MAX_NOA_DESCRIPTORS) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid NOA "
				   "subelement too large (length %d)\n", __func__, len);
            return -1;
        }
		msg->noa = data;
		msg->noa_num_descriptors = len;
        break;
	case IEEE80211_P2P_SUB_ELEMENT_DEVICE_INFO:
		if (len < IEEE80211_ADDR_LEN + 2 + 8 + 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Device Info "
				   "subelement (length %d)\n", __func__, len);
			return -1;
		}
		msg->p2p_device_info = data;
		msg->p2p_device_info_len = len;
		pos = data;
		msg->p2p_device_addr = pos;
		pos += IEEE80211_ADDR_LEN;
		msg->config_methods = P2PIE_GET_BE16(pos);
		pos += 2;
		msg->pri_dev_type = pos;
		pos += 8;
		msg->num_sec_dev_types = *pos++;
		if (msg->num_sec_dev_types * 8 > data + len - pos) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Device Info underflow "
                              "length %d\n", __func__, len);
			return -1;
		}
		pos += msg->num_sec_dev_types * 8;
		if (data + len - pos < 4) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Device Name "
				   "length %d\n", __func__, (int)(data + len - pos));
			return -1;
		}
		if (P2PIE_GET_BE16(pos) != ATTR_DEV_NAME) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Unexpected Device Name "
                              "val %x != ATTR_DEV_NAME %x\n",
                              __func__, P2PIE_GET_BE16(pos), ATTR_DEV_NAME);
			return -1;
		}
		pos += 2;
		nlen = P2PIE_GET_BE16(pos);
		pos += 2;
		if (data + len - pos < (int) nlen || nlen > 32) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Device Name "
				   "length %d (buf len %d)\n", __func__,  (int) nlen,
				   (int) (data + len - pos));
			return -1;
		}
		OS_MEMCPY(msg->device_name, pos, nlen);
		for (i = 0; i < nlen; i++) {
			if (msg->device_name[i] == '\0')
				break;
			if (msg->device_name[i] < 32)
				msg->device_name[i] = '_';
		}
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Device Info: addr " MACSTR
			   " primary device type %u-%08X-%u device name '%s' "
			   "config methods 0x%x\n", __func__,
			   MAC2STR(msg->p2p_device_addr),
               P2PIE_GET_BE16(msg->pri_dev_type),
               P2PIE_GET_BE32(&msg->pri_dev_type[2]),
               P2PIE_GET_BE16(&msg->pri_dev_type[6]),
			   msg->device_name, msg->config_methods);
		break;
	case IEEE80211_P2P_SUB_ELEMENT_GROUP_INFO:
		msg->group_info = data;
		msg->group_info_len = len;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Group Info "
                          "length %d\n", __func__, len);
		break;
    case IEEE80211_P2P_SUB_ELEMENT_GROUP_ID:
        if (len < ETH_ALEN || len > ETH_ALEN + 32) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid P2P Group ID "
				   "attribute length %d", __func__, len);
			return -1;
		}
        break;
    /* case IEEE80211_P2P_SUB_ELEMENT_INTERFACE: */
    /* case IEEE80211_P2P_SUB_ELEMENT_VENDOR: */
	default:
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Skipped unknown subelement %d "
			   "(length %d)\n", __func__, id, len);
		break;
	}

	return 0;
}


/**
 * ieee80211_p2p_parse_p2p_ie - Parse P2P IE
 * @vap: handle for IEEE80211_DPRINTF
 * @buf: Concatenated P2P IE(s) payload
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller is responsible for clearing the msg data structure before
 * calling this function.
 */
static int
ieee80211_p2p_parse_p2p_ie(wlan_if_t vap, const struct iebuf *buf,
                           struct p2p_parsed_ie *msg)
{
	const u8 *pos = iebuf_head_u8(buf);
	const u8 *end = pos + iebuf_len(buf);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Parsing P2P IE pos %p end %p\n",
                      __func__, pos, end);

	while (pos < end) {
		u16 attr_len;
		if (pos + 2 >= end) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid P2P subelement "
                              "pos + 2 %p >= end %p\n", __func__, pos+2, end);
			return -1;
		}
		attr_len = P2PIE_GET_LE16(pos + 1);
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Subelement %d length %u\n",
			   __func__, pos[0], attr_len);
		if (pos + 3 + attr_len > end) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Subelement underflow "
				   "(len=%u left=%d)\n", __func__,
				   attr_len, (int) (end - pos - 3));
			return -1;
		}
		if (ieee80211_p2p_parse_subelement(vap, pos[0], pos + 3, attr_len, msg))
			return -1;
		pos += 3 + attr_len;
	}

	return 0;
}



static int
wps_set_attr(wlan_if_t vap, struct wps_parsed_attr *attr, u16 type,
			const u8 *pos, u16 len)
{
	switch (type) {
	case ATTR_CONFIG_METHODS:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Config Methods "
				   "length %u\n", __func__, len);
			return -1;
		}
		attr->config_methods = pos;
		break;
	case ATTR_DEV_NAME:
		attr->dev_name = pos;
		attr->dev_name_len = len;
		break;
    case ATTR_VERSION:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Version "
				   "length %u", __func__, len);
			return -1;
		}
		attr->version = pos;
		break;
	case ATTR_MSG_TYPE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Message Type "
				   "length %u", __func__, len);
			return -1;
		}
		attr->msg_type = pos;
		break;
	case ATTR_ENROLLEE_NONCE:
		if (len != WPS_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Enrollee Nonce "
				   "length %u", __func__, len);
			return -1;
		}
		attr->enrollee_nonce = pos;
		break;
	case ATTR_REGISTRAR_NONCE:
		if (len != WPS_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Registrar Nonce "
				   "length %u", __func__, len);
			return -1;
		}
		attr->registrar_nonce = pos;
		break;
	case ATTR_UUID_E:
		if (len != WPS_UUID_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid UUID-E "
				   "length %u", __func__, len);
			return -1;
		}
		attr->uuid_e = pos;
		break;
	case ATTR_UUID_R:
		if (len != WPS_UUID_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid UUID-R "
				   "length %u", __func__, len);
			return -1;
		}
		attr->uuid_r = pos;
		break;
	case ATTR_AUTH_TYPE_FLAGS:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Authentication "
				   "Type Flags length %u", __func__, len);
			return -1;
		}
		attr->auth_type_flags = pos;
		break;
	case ATTR_ENCR_TYPE_FLAGS:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Encryption Type "
				   "Flags length %u", __func__, len);
			return -1;
		}
		attr->encr_type_flags = pos;
		break;
	case ATTR_CONN_TYPE_FLAGS:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Connection Type "
				   "Flags length %u", __func__, len);
			return -1;
		}
		attr->conn_type_flags = pos;
		break;
	case ATTR_SELECTED_REGISTRAR_CONFIG_METHODS:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Selected "
				   "Registrar Config Methods length %u", __func__, len);
			return -1;
		}
		attr->sel_reg_config_methods = pos;
		break;
	case ATTR_PRIMARY_DEV_TYPE:
		if (len != WPS_DEV_TYPE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Primary Device "
				   "Type length %u", __func__, len);
			return -1;
		}
		attr->primary_dev_type = pos;
		break;
	case ATTR_RF_BANDS:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid RF Bands length "
				   "%u", __func__, len);
			return -1;
		}
		attr->rf_bands = pos;
		break;
	case ATTR_ASSOC_STATE:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Association State "
				   "length %u", __func__, len);
			return -1;
		}
		attr->assoc_state = pos;
		break;
	case ATTR_CONFIG_ERROR:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Configuration "
				   "Error length %u", __func__, len);
			return -1;
		}
		attr->config_error = pos;
		break;
	case ATTR_DEV_PASSWORD_ID:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Device Password "
				   "ID length %u", __func__, len);
			return -1;
		}
		attr->dev_password_id = pos;
		break;
	case ATTR_OOB_DEVICE_PASSWORD:
		if (len != WPS_OOB_DEVICE_PASSWORD_ATTR_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid OOB Device "
				   "Password length %u", __func__, len);
			return -1;
		}
		attr->oob_dev_password = pos;
		break;
	case ATTR_OS_VERSION:
		if (len != 4) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid OS Version length "
				   "%u", __func__, len);
			return -1;
		}
		attr->os_version = pos;
		break;
	case ATTR_WPS_STATE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Wi-Fi Protected "
				   "Setup State length %u", __func__, len);
			return -1;
		}
		attr->wps_state = pos;
		break;
	case ATTR_AUTHENTICATOR:
		if (len != WPS_AUTHENTICATOR_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Authenticator "
				   "length %u", __func__, len);
			return -1;
		}
		attr->authenticator = pos;
		break;
	case ATTR_R_HASH1:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-Hash1 length %u",
				   __func__, len);
			return -1;
		}
		attr->r_hash1 = pos;
		break;
	case ATTR_R_HASH2:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-Hash2 length %u",
				   __func__, len);
			return -1;
		}
		attr->r_hash2 = pos;
		break;
	case ATTR_E_HASH1:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-Hash1 length %u",
				   __func__, len);
			return -1;
		}
		attr->e_hash1 = pos;
		break;
	case ATTR_E_HASH2:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-Hash2 length %u",
				   __func__, len);
			return -1;
		}
		attr->e_hash2 = pos;
		break;
	case ATTR_R_SNONCE1:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-SNonce1 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->r_snonce1 = pos;
		break;
	case ATTR_R_SNONCE2:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-SNonce2 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->r_snonce2 = pos;
		break;
	case ATTR_E_SNONCE1:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-SNonce1 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->e_snonce1 = pos;
		break;
	case ATTR_E_SNONCE2:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-SNonce2 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->e_snonce2 = pos;
		break;
	case ATTR_KEY_WRAP_AUTH:
		if (len != WPS_KWA_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Key Wrap "
				   "Authenticator length %u", __func__, len);
			return -1;
		}
		attr->key_wrap_auth = pos;
		break;
	case ATTR_AUTH_TYPE:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Authentication "
				   "Type length %u", __func__, len);
			return -1;
		}
		attr->auth_type = pos;
		break;
	case ATTR_ENCR_TYPE:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Encryption "
				   "Type length %u", __func__, len);
			return -1;
		}
		attr->encr_type = pos;
		break;
	case ATTR_NETWORK_INDEX:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Network Index "
				   "length %u", __func__, len);
			return -1;
		}
		attr->network_idx = pos;
		break;
	case ATTR_NETWORK_KEY_INDEX:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Network Key Index "
				   "length %u", __func__, len);
			return -1;
		}
		attr->network_key_idx = pos;
		break;
	case ATTR_MAC_ADDR:
		if (len != ETH_ALEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid MAC Address "
				   "length %u", len);
			return -1;
		}
		attr->mac_addr = pos;
		break;
	case ATTR_KEY_PROVIDED_AUTO:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Key Provided "
				   "Automatically length %u", __func__, len);
			return -1;
		}
		attr->key_prov_auto = pos;
		break;
	case ATTR_802_1X_ENABLED:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid 802.1X Enabled "
				   "length %u", __func__, len);
			return -1;
		}
		attr->dot1x_enabled = pos;
		break;
	case ATTR_SELECTED_REGISTRAR:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Selected Registrar"
				   " length %u", __func__, len);
			return -1;
		}
		attr->selected_registrar = pos;
		break;
	case ATTR_REQUEST_TYPE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Request Type "
				   "length %u", __func__, len);
			return -1;
		}
		attr->request_type = pos;
		break;
	case ATTR_RESPONSE_TYPE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Response Type "
				   "length %u", __func__, len);
			return -1;
		}
		attr->response_type = pos;
		break;
	case ATTR_MANUFACTURER:
		attr->manufacturer = pos;
		attr->manufacturer_len = len;
		break;
	case ATTR_MODEL_NAME:
		attr->model_name = pos;
		attr->model_name_len = len;
		break;
	case ATTR_MODEL_NUMBER:
		attr->model_number = pos;
		attr->model_number_len = len;
		break;
	case ATTR_SERIAL_NUMBER:
		attr->serial_number = pos;
		attr->serial_number_len = len;
		break;      
	case ATTR_PUBLIC_KEY:
		attr->public_key = pos;
		attr->public_key_len = len;
		break;
	case ATTR_ENCR_SETTINGS:
		attr->encr_settings = pos;
		attr->encr_settings_len = len;
		break;
	case ATTR_CRED:
		if (attr->num_cred >= MAX_CRED_COUNT) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Skipped Credential "
				   "attribute (max %d credentials)", __func__, 
				   MAX_CRED_COUNT);
			break;
		}
		attr->cred[attr->num_cred] = pos;
		attr->cred_len[attr->num_cred] = len;
		attr->num_cred++;
		break;
	case ATTR_SSID:
		attr->ssid = pos;
		attr->ssid_len = len;
		break;
	case ATTR_NETWORK_KEY:
		attr->network_key = pos;
		attr->network_key_len = len;
		break;
	case ATTR_EAP_TYPE:
		attr->eap_type = pos;
		attr->eap_type_len = len;
		break;
	case ATTR_EAP_IDENTITY:
		attr->eap_identity = pos;
		attr->eap_identity_len = len;
		break;
	case ATTR_AP_SETUP_LOCKED:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid AP Setup Locked "
				   "length %u", __func__, len);
			return -1;
		}
		attr->ap_setup_locked = pos;
		break;
	case ATTR_REQUESTED_DEV_TYPE:
		if (len != WPS_DEV_TYPE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Requested Device "
				   "Type length %u", __func__, len);
			return -1;
		}
		if (attr->num_req_dev_type >= MAX_REQ_DEV_TYPE_COUNT) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Skipped Requested Device "
				   "Type attribute (max %u types)", __func__, 
				   MAX_REQ_DEV_TYPE_COUNT);
			break;
		}
		attr->req_dev_type[attr->num_req_dev_type] = pos;
		attr->num_req_dev_type++;
		break;
	case ATTR_VENDOR_EXT:
		break;        
	default:
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT,
               "%s: Unsupported attribute type 0x%x "
			   "len=%u\n", __func__, __func__, type, len);
		break;
	}

	return 0;
}


static int
wps_parse_msg(wlan_if_t vap, const struct iebuf *msg, struct wps_parsed_attr *attr)
{
	const u8 *pos, *end;
	u16 type, len;

	OS_MEMZERO(attr, sizeof(*attr));
	pos = iebuf_head(msg);
	end = pos + iebuf_len(msg);

	while (pos < end) {
		if (end - pos < 4) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid message - "
				   "%lu bytes remaining\n", __func__, (unsigned long) (end - pos));
			return -1;
		}

		type = P2PIE_GET_BE16(pos);
		pos += 2;
		len = P2PIE_GET_BE16(pos);
		pos += 2;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: attr type=0x%x len=%u\n",
			   __func__, type, len);
		if (len > end - pos) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Attribute overflow\n", __func__);
			return -1;
		}

		if (wps_set_attr(vap, attr, type, pos, len) < 0)
			return -1;

		pos += len;
	}

	return 0;
}

static int
ieee80211_p2p_parse_wps_ie(wlan_if_t vap, const struct iebuf *buf, struct p2p_parsed_ie *msg)
{
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Parsing WPS IE\n", __func__);
	if (wps_parse_msg(vap, buf, &msg->wps_attributes_parsed))
		return -1;
	if (msg->wps_attributes_parsed.dev_name && msg->wps_attributes_parsed.dev_name_len < sizeof(msg->device_name) &&
	    !msg->device_name[0])
		OS_MEMCPY(msg->device_name, msg->wps_attributes_parsed.dev_name, msg->wps_attributes_parsed.dev_name_len);
	else if (msg->wps_attributes_parsed.config_methods)
		msg->config_methods = P2PIE_GET_BE16(msg->wps_attributes_parsed.config_methods);
	return 0;
}

/**
 * ieee80211_p2p_parse_free - Free temporary data from P2P parsing
 * @msg: Parsed attributes
 */
void
ieee80211_p2p_parse_free(struct p2p_parsed_ie *msg)
{
    if (msg) {
        iebuf_free(msg->p2p_attributes);
        msg->p2p_attributes = NULL;

        iebuf_free(msg->wps_attributes);
        msg->wps_attributes = NULL;        
    }
}

/**
 * ieee80211_p2p_parse_ies - Parse P2P message IEs (both WPS and P2P IE)
 * @osdev: handle for malloc
 * @vap: handle for IEEE80211_DPRINTF
 * @data: IEs from the message
 * @len: Length of data buffer in octets
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller is responsible for clearing the msg data structure before
 * calling this function.
 *
 * Note: Caller must free temporary memory allocations by calling
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
int
ieee80211_p2p_parse_ies(osdev_t oshandle, wlan_if_t vap,
                        const u8 *data, size_t len, struct p2p_parsed_ie *msg)
{
    const u8 wps_oui[3] = IEEE80211_WPS_WFA_OUI;
    const u8 p2p_oui[3] = IEEE80211_P2P_WFA_OUI;
    const u8                    *ssid;
    const u8                    *frm, *efrm;
    struct ieee80211_rateset    *rs = &(msg->rateset);
    int                         i;

    if (msg == NULL)
        return -1;

    /* Check the SSID and collect the rates */
    ssid = NULL;
    rs->rs_nrates = 0;
    frm = data;
    efrm = frm + len;
    while (((frm+1) < efrm) && (frm + frm[1] + 1 < efrm)) {

        switch (*frm) {
        case IEEE80211_ELEMID_RATES:
        case IEEE80211_ELEMID_XRATES:
            for (i = 0; i < frm[1]; i++) {
                if (rs->rs_nrates == IEEE80211_RATE_MAXSIZE)
                    break;
                rs->rs_rates[rs->rs_nrates] = frm[1 + i] & IEEE80211_RATE_VAL;
                rs->rs_nrates++;
            }
            break;
        case IEEE80211_ELEMID_SSID:
            ssid = frm;
            break;
        }

        frm += frm[1] + 2;
    }

    if (frm > efrm) {
        printk("%s: Err: IE's buf misaligned\n", __func__);
    }

    if (ssid) {
        /* Check if SSID is a wildcard P2P IE */
        msg->ssid = ssid;

        if ((ssid[1] == IEEE80211_P2P_WILDCARD_SSID_LEN) &&
            (OS_MEMCMP(ssid + 2, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN) == 0))
        {
            msg->is_p2p_wildcard_ssid = 1;
        }
    }
	msg->wps_attributes = ieee802_11_vendor_ie_concat(oshandle, data, len, 
                               wps_oui, IEEE80211_P2P_WPS_VER);
	if (msg->wps_attributes &&
	    ieee80211_p2p_parse_wps_ie(vap, msg->wps_attributes, msg)) {
            ieee80211_p2p_parse_free(msg);
            return -1;
    }

	msg->p2p_attributes = ieee802_11_vendor_ie_concat(oshandle, data, len,
							   p2p_oui, IEEE80211_P2P_WFA_VER);
	if (msg->p2p_attributes &&
	    ieee80211_p2p_parse_p2p_ie(vap, msg->p2p_attributes, msg)) {
		ieee80211_p2p_parse_free(msg);
		return -1;
	}

	return 0;
}

#if 0
/**
 * ieee80211_p2p_parse - Parse a P2P Action frame contents
 * @data: Action frame payload after Category and Code fields
 * @len: Length of data buffer in octets
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller must free temporary memory allocations by calling
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
int ieee80211_p2p_parse(wlan_if_t vap, const u8 *data, size_t len, struct p2p_parsed_ie *msg)
{
	OS_MEMZERO(msg, sizeof(*msg));
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Parsing the received message\n", __func__);
	if (len < 1) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: No Dialog Token in the message\n", __func__);
		return -1;
	}
	msg->dialog_token = data[0];
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Dialog Token: %d\n", __func__,  msg->dialog_token);

	return ieee80211_p2p_parse_ies(vap, data + 1, len - 1, msg);
}
#endif

#endif
