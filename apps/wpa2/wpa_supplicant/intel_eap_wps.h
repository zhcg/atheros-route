
#ifndef EAP_WPS_H
#define EAP_WPS_H

#pragma pack(push, 1)

#define WPS_RECVBUF_SIZE    2048

struct eap_wps_data {
    enum { START, CONTINUE, SUCCESS, FAILURE } state;
    int udpFdEap;
    int udpFdCom;
    u8 recvBuf[WPS_RECVBUF_SIZE];
};

#define WPS_NOTIFY_TYPE_BUILDREQ              1
#define WPS_NOTIFY_TYPE_BUILDREQ_RESULT       2
#define WPS_NOTIFY_TYPE_PROCESS_REQ           3
#define WPS_NOTIFY_TYPE_PROCESS_RESP          4
#define WPS_NOTIFY_TYPE_PROCESS_RESULT        5

#define WPS_NOTIFY_RESULT_SUCCESS			  0x00
#define WPS_NOTIFY_RESULT_FAILURE			  0xFF

typedef struct wps_notify_buildreq_tag {
    u32    id;
    u32 state;
} WPS_NOTIFY_BUILDREQ;

typedef struct wps_notify_process_buildreq_result_tag {
	u8 result;
} WPS_NOTIFY_BUILDREQ_RESULT;

typedef struct wps_notify_process_tag {
	u32 state;
} WPS_NOTIFY_PROCESS;

typedef struct wps_notify_process_result_tag {
	u8 result;
	u8 done;
} WPS_NOTIFY_PROCESS_RESULT;

typedef struct wps_notify_data_tag {
    u8 type;
    union {
        WPS_NOTIFY_BUILDREQ bldReq;
        WPS_NOTIFY_BUILDREQ_RESULT bldReqResult;
        WPS_NOTIFY_PROCESS process;
        WPS_NOTIFY_PROCESS_RESULT processResult;
    } u;
	u32 length; // length of the data that follows
} WPS_NOTIFY_DATA;


/*
#define WPS_CTYPE_NEW_SETTINGS      1
#define WPS_CTYPE_MAX               2

#define WPS_MAX_SSID_LEN            32
#define WPS_MAX_PSK_LEN             32

struct wps_command_data {
    u8 type;
    union {
        struct wps_new_settings
        {
            u16 ssidLen;
            char ssid[WPS_MAX_SSID_LEN];
            u16 pskLen;
            u8 psk[WPS_MAX_PSK_LEN];
        } new_set;
    } u;
};
*/

#pragma pack(pop)

#endif /*EAP_WPS_H*/
