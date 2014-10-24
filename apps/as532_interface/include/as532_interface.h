#ifndef __AS532_INT__
#define	__AS532_INT__

//532 update
#define B6L_UART_NAME			"/dev/ttyS1"
#define A20_UART_NAME			"/dev/ttyS2"
#define AS532_HEAD1			0XA5
#define AS532_HEAD2			0X5A
#define CMD_HEAD			0XEF01
#define GETVER				0X02
#define GETSN				0X03
#define GETVERDES			0X05
#define KEYCMD				0X06
#define JUMPBOOT			0X04
#define CHECKSTATUS			0X07

#define RSP_CMD_MIN_BYTES 	0x07
#define REQ_RSP_OK			0x00
#define B6L_DEFAULT_AS532_IMAGE	"/var/default_image/as532h/b6l_as532h_default_image"
#define A20_DEFAULT_AS532_IMAGE	"/var/default_image/as532h/HBD_F2B_AS532.bin"
#define REMOTE_SERVER_PORT	6000
#define PROTOCOL_VER		"0101"
#define REQUEST_TYPE		0x01
#define RESPOND_TYPE		0x02
#define STATUS_TYPE			0x03
//#define B6L_AS532_PACKET_NAME	"HBD_B6L_AS532"
//#define A20_AS532_PACKET_NAME	"HBD_F2B_S1AS532"
#define AS532_VER_NUM		"\"versionNum\":\"V"
#define AS532_VER_NUM_NOV		"\"versionNum\":\""
#define AS532_VER_PATH		"\"versionPath\":\""
#define AS532_CONF_MD5		"\"checkCode\":\""
#define WGET				"/bin/wget"
#define DOWNLOAD_AS532_FILE "/tmp/AS532.bin"
#define DOWNLOAD_AS532_TEMP_CONF_FILE	"/etc/532_tmp.conf"
#define DOWNLOAD_AS532_CONF_FILE	"/etc/532.conf"

#define F2B_INFO_FILE				"/var/.f2b_info.conf"

#define VPN_CRT_USER				"/etc/crt/author-keys"


typedef struct _AS532_KEY_DATA_STR
{
	unsigned char appName[32];
	unsigned char vesselName[32];
	unsigned char vesselId;
	unsigned char appId;
	unsigned char p1;
	unsigned char p2;
	unsigned char pubKey[514];
}AS532_KEY_DATA, *pAS532_KEY_DATA;

typedef struct _F2B_INFO_DATA_STR
{
	unsigned char baseSn[20];
	unsigned char mac[20];
	unsigned char baseVer[64];
	unsigned char keySn[64];
	unsigned char keyCount;
	AS532_KEY_DATA keyData[4];
}F2B_INFO_DATA, *pF2B_INFO_DATA;


#endif
