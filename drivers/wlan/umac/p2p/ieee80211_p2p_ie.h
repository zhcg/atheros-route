/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */

/*
 * P2P Protocol ie operation.
 */

#ifndef IEEE80211_P2P_IE_H
#define IEEE80211_P2P_IE_H

u_int8_t *ieee80211_p2p_add_p2p_ie(u_int8_t *frm);

/*
* parse NOA sub element.
*/
int ieee80211_p2p_parse_noa(const u_int8_t *frm, u_int8_t len, struct ieee80211_p2p_sub_element_noa *noa);

/*
* Add NOA sub element.
*/
u_int8_t *ieee80211_p2p_add_noa(u_int8_t *frm, struct ieee80211_p2p_sub_element_noa *noa, u_int32_t tsf_offset);

/**
 * struct p2p_parsed_ie - Parsed P2P message (or P2P IE)
 */
struct p2p_parsed_ie {
	struct iebuf *p2p_attributes;

    struct iebuf *wps_attributes;
    struct wps_parsed_attr wps_attributes_parsed;

	u8 dialog_token;

	const u8 *ssid;
    int      is_p2p_wildcard_ssid:1;
    struct ieee80211_rateset rateset;

	const u8 *capability;
	const u8 *go_intent;
	const u8 *status;
	const u8 *channel;
	const u8 *channel_list;
	u8 channel_list_len;
	const u8 *config_timeout;
	const u8 *intended_addr;

	const u8 *group_info;
	size_t group_info_len;
	const u8 *device_id;   

    const u8 *noa;
	u8 noa_num_descriptors;

	/* P2P Device Info */
	const u8 *p2p_device_info;
	size_t p2p_device_info_len;
	const u8 *p2p_device_addr;
	const u8 *pri_dev_type;
	u8 num_sec_dev_types;
	char device_name[33];
	u16 config_methods;
};

#define P2P_MAX_GROUP_ENTRIES 50

struct p2p_group_info {
    unsigned int num_clients;
    struct p2p_client_info {
        const u8 *p2p_device_addr;
        const u8 *p2p_interface_addr;
        u8 dev_capab;
        u16 config_methods;
        const u8 *pri_dev_type;
        u8 num_sec_dev_types;
        const u8 *sec_dev_types;
        const char *dev_name;
        size_t dev_name_len;
    } client[P2P_MAX_GROUP_ENTRIES];
};

int ieee80211_p2p_parse_group_info(const u8 *gi, size_t gi_len, struct p2p_group_info *info);

/*
* Free P2P IE buffer created in ieee80211_p2p_parse_ies
*/
void ieee80211_p2p_parse_free(struct p2p_parsed_ie *msg);

/*
* Parse P2P IE elements into p2p_parsed_ie struct
*/
int ieee80211_p2p_parse_ies(osdev_t oshandle, wlan_if_t vap, const u8 *data, size_t len, struct p2p_parsed_ie *msg);


/* Macros for handling unaligned memory accesses */
#define P2PIE_GET_BE16(a) ((u16) (((a)[0] << 8) | (a)[1]))
#define P2PIE_PUT_BE16(a, val)			\
	do {					\
		(a)[0] = ((u16) (val)) >> 8;	\
		(a)[1] = ((u16) (val)) & 0xff;	\
	} while (0)

#define P2PIE_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))
#define P2PIE_PUT_LE16(a, val)			\
	do {					\
		(a)[1] = ((u16) (val)) >> 8;	\
		(a)[0] = ((u16) (val)) & 0xff;	\
	} while (0)

#define P2PIE_GET_BE24(a) ((((u32) (a)[0]) << 16) | (((u32) (a)[1]) << 8) | \
			 ((u32) (a)[2]))
#define P2PIE_PUT_BE24(a, val)					\
	do {							\
		(a)[0] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[2] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define P2PIE_GET_BE32(a) ((((u32) (a)[0]) << 24) | (((u32) (a)[1]) << 16) | \
			 (((u32) (a)[2]) << 8) | ((u32) (a)[3]))
#define P2PIE_PUT_BE32(a, val)					\
	do {							\
		(a)[0] = (u8) ((((u32) (val)) >> 24) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[2] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[3] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define P2PIE_GET_LE32(a) ((((u32) (a)[3]) << 24) | (((u32) (a)[2]) << 16) | \
			 (((u32) (a)[1]) << 8) | ((u32) (a)[0]))
#define P2PIE_PUT_LE32(a, val)					\
	do {							\
		(a)[3] = (u8) ((((u32) (val)) >> 24) & 0xff);	\
		(a)[2] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[0] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define P2PIE_GET_BE64(a) ((((u64) (a)[0]) << 56) | (((u64) (a)[1]) << 48) | \
			 (((u64) (a)[2]) << 40) | (((u64) (a)[3]) << 32) | \
			 (((u64) (a)[4]) << 24) | (((u64) (a)[5]) << 16) | \
			 (((u64) (a)[6]) << 8) | ((u64) (a)[7]))
#define P2PIE_PUT_BE64(a, val)				\
	do {						\
		(a)[0] = (u8) (((u64) (val)) >> 56);	\
		(a)[1] = (u8) (((u64) (val)) >> 48);	\
		(a)[2] = (u8) (((u64) (val)) >> 40);	\
		(a)[3] = (u8) (((u64) (val)) >> 32);	\
		(a)[4] = (u8) (((u64) (val)) >> 24);	\
		(a)[5] = (u8) (((u64) (val)) >> 16);	\
		(a)[6] = (u8) (((u64) (val)) >> 8);	\
		(a)[7] = (u8) (((u64) (val)) & 0xff);	\
	} while (0)

#define P2PIE_GET_LE64(a) ((((u64) (a)[7]) << 56) | (((u64) (a)[6]) << 48) | \
			 (((u64) (a)[5]) << 40) | (((u64) (a)[4]) << 32) | \
			 (((u64) (a)[3]) << 24) | (((u64) (a)[2]) << 16) | \
			 (((u64) (a)[1]) << 8) | ((u64) (a)[0]))

/*
 * Internal data structure for iebuf. Please do not touch this directly from
 * elsewhere. This is only defined in header file to allow inline functions
 * from this file to access data.
 */
struct iebuf {
	size_t size; /* total size of the allocated buffer */
	size_t used; /* length of data in the buffer */
	/* optionally followed by the allocated buffer */
};

/**
 * iebuf_len - Get the current length of a iebuf buffer data
 * @buf: iebuf buffer
 * Returns: Currently used length of the buffer
 */
static inline size_t
iebuf_len(const struct iebuf *buf)
{
	return buf->used;
}

/**
 * iebuf_head - Get pointer to the head of the buffer data
 * @buf: iebuf buffer
 * Returns: Pointer to the head of the buffer data
 */
static inline const void *
iebuf_head(const struct iebuf *buf)
{
	return buf + 1;
}

static inline const u8 *
iebuf_head_u8(const struct iebuf *buf)
{
	return iebuf_head(buf);
}

/**
 * iebuf_mhead - Get modifiable pointer to the head of the buffer data
 * @buf: iebuf buffer
 * Returns: Pointer to the head of the buffer data
 */
static inline void *
iebuf_mhead(struct iebuf *buf)
{
	return buf + 1;
}

static inline u8 *
iebuf_mhead_u8(struct iebuf *buf)
{
	return iebuf_mhead(buf);
}

static void
iebuf_overflow(const struct iebuf *buf, size_t len)
{
    KASSERT(0, ("iebuf %p (size=%lu used=%lu) overflow len=%lu",
		   buf, (unsigned long)buf->size, (unsigned long)buf->used,
		   (unsigned long)len));
}

static inline void *
iebuf_put(struct iebuf *buf, size_t len)
{
	void *tmp = iebuf_mhead_u8(buf) + iebuf_len(buf);
	buf->used += len;
	if (buf->used > buf->size) {
		iebuf_overflow(buf, len);
	}
	return tmp;
}

static inline void
iebuf_put_data(struct iebuf *buf, const void *data, size_t len)
{
	if (data)
		OS_MEMCPY(iebuf_put(buf, len), data, len);
}

static inline struct iebuf *
iebuf_alloc(osdev_t pNicDev, size_t len)
{
	struct iebuf *buf = (struct iebuf *)OS_MALLOC(pNicDev, sizeof(struct iebuf) + len, 0);
	if (buf == NULL)
		return NULL;
    OS_MEMZERO(buf, sizeof(struct iebuf) + len);
	buf->size = len;
	return buf;
}

static inline void
iebuf_free(struct iebuf *buf)
{
	if (buf == NULL)
		return;
	OS_FREE(buf);
}

#endif
