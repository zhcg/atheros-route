#ifndef INTEL_IE_WPS_H
#define INTEL_IE_WPS_H

#pragma pack(push, 1)

typedef struct wps_ie_data {
	struct wpa_supplicant *wpa_s;
	int udpFdCom;
	int sendCounter;
	int sendUp;
} WPS_IE_DATA;

#if !defined(DIFF_PORT_FROM_HOSTAPD)
#define WPS_WLAN_UDP_PORT       38000
#else   // !defined(DIFF_PORT_FROM_HOSTAPD)
#define WPS_WLAN_UDP_PORT       38002
#endif  // !defined(DIFF_PORT_FROM_HOSTAPD)
#define WPS_WLAN_UDP_ADDR       "127.0.0.1"

#define WPS_WLAN_DATA_MAX_LENGTH         1024

#define WPS_IE_TYPE_SET_BEACON_IE     			1
#define WPS_IE_TYPE_SET_PROBE_REQUEST_IE     	2
#define WPS_IE_TYPE_SET_PROBE_RESPONSE_IE     	3
#define WPS_IE_TYPE_BEACON_IE_DATA     			4
#define WPS_IE_TYPE_PROBE_REQUEST_IE_DATA     	5
#define WPS_IE_TYPE_PROBE_RESPONSE_IE_DATA     	6
#define WPS_IE_TYPE_SEND_BEACONS_UP				7
#define WPS_IE_TYPE_SEND_PR_RESPS_UP			8
#define WPS_IE_TYPE_SEND_PROBE_REQUEST			9
#define WPS_IE_TYPE_MAX                         10

typedef struct wps_ie_command_data {
	u8 type;
	u32 length;
	u8 data[];
} WPS_IE_COMMAND_DATA;

typedef struct wps_ie_beacon_data
{
	char        ssid[32];
	u8          macAddr[6];
	u8          data[];
} WPS_IE_BEACON_DATA;


int wps_ie_init(struct wpa_supplicant *wpa_s);
int wps_ie_deinit(struct wpa_supplicant *wpa_s);
 
#pragma pack(pop)

#endif /* INTEL_IE_WPS_H */

