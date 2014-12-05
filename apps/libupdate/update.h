#ifndef __AS532_INT__
#define	__AS532_INT__

#define PROTOCOL_VER				"0101"
#define REQUEST_TYPE				0x01
#define RESPOND_TYPE				0x02
#define STATUS_TYPE					0x03
#define VER_NUM						"\"versionNum\":\"V"
#define VER_NUM_NOV						"\"versionNum\":\""
#define VER_DESC					"\"versionDesc\":\""
#define VER_PATH					"\"versionPath\":\""
#define CONF_MD5					"\"checkCode\":\""
#define WGET						"/bin/wget"

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
