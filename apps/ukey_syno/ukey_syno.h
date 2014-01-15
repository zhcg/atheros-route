#ifndef SCSIEXE_H_
#define SCSIEXE_H_

#include <scsi/sg.h>
#include <sys/ioctl.h>


#define SENSE_LEN 	255
#define BLOCK_LEN 	32
#define SCSI_TIMEOUT	20000

//算法模式：
#define SGD_SM1_ECB  	0x00000101  //SM1 算法 ECB加密模式 
#define SGD_SM1_CBC  	0x00000102  //SM1 算法 CBC加密模式 
#define SGD_SM1_MAC  	0x00000110  //SM1 算法 MAC运算 
#define SGD_SMS4_ECB  	0x00000401  //SMS4算法 ECB加密模式 
#define SGD_SMS4_CBC  	0x00000402  //SMS4算法 CBC加密模式 
#define SGD_SMS4_MAC  	0x00000410  //SMS4算法 MAC运算

#define MAX_IV_LEN 32 


struct	TPCCmd{
		unsigned short int CMD;	//SCSI 命令
		unsigned char CLA;		//命令类别	
		unsigned char INS;		//命令	
		unsigned char P1;		//参数1	
		unsigned char P2;		//参数2	
		unsigned short int LC;	//发送的数据包长度	
		unsigned short int LE;	//希望接收的最大数据包长度
		};

struct 	TRespond{
		unsigned short int CMD;	//SCSI 命令
		unsigned short int SW;	//传输响应数据
		};

struct 	TDataPack{
		unsigned short int LC;	//数据包长度
		unsigned char * DATA;	//数据
		};

struct	UKey{
		int fd;
		struct TPCCmd localTPCCmd;
		struct TRespond localTRespond;
		struct TDataPack localTDataPack;
		struct sg_io_hdr * p_hdr;
		};

struct AppData{
		char AppName[48];
		unsigned char AdminPIN[16];		
		unsigned int AdminTry; 
		unsigned char UserPIN[16];		
		unsigned int UserTry; 
		unsigned char Permission[8];
	};
struct AppAttribute{
		char *pAppName;
		unsigned int AppNameLen;
		unsigned short AppID;
	};

struct ContAttribute{
		
		unsigned int ContNameLen;
		char *pContName;
		unsigned short ContID;
		
	};
struct PinAttribute{
		unsigned char Pin[16];
		unsigned int PinLen;
		unsigned char PinType;
	};
struct FileAttribute{
		char FileName[48];
		unsigned int FileSize;
		unsigned int ReadRights;
		unsigned int WriteRights;
	};
struct FileData{
		char *pFileName;
		unsigned short FileNameLen;
		unsigned char *pDataAddr;
		unsigned int DataOffset;
		unsigned short DataLength;
	};
struct FileInfo{
		char FileName[20];
		unsigned int FileNameLen;
		unsigned int FileType;
		unsigned int FileLen;
	};

struct KeyAttribute{
	unsigned char *pKey;
	unsigned int KeyLen;
	unsigned char KeyUse;
	unsigned short KeyID;
	};
struct CertAttribute{
	unsigned char *pCert;
	unsigned int CertLen;
	unsigned char CertUse;
	};
struct OptData{
	unsigned char *pPlainData;
	unsigned int PlainDataLen;
	unsigned char *pCipherData;
	unsigned int CipherDataLen;
	};
struct BlockCipherParam{
	unsigned char IV[MAX_IV_LEN];
	unsigned int IVLen;
	unsigned int PaddingType;
	unsigned int FeedBitLen;
	};
struct 	HashData{
		unsigned int len;	//数据包长度
		unsigned char * data;	//数据
		};


int UsbOpen(struct UKey *pUKey, unsigned char *pRandom, unsigned char *pUsbData);

int GenRandom(struct UKey *pUKey, unsigned char *pRandom);
int GetKeyParam(struct UKey *pUKey, unsigned char *pKeyParam);

int AuthDevKey(struct UKey *pUKey, unsigned char *pAuthData);
int NewApp(struct UKey *pUKey, struct AppData *pAppData, 
			struct AppAttribute *pAppAttribute);
int OpenApp(struct UKey *pUKey, struct AppAttribute *pAppAttribute);
int CloseApp(struct UKey *pUKey, struct AppAttribute *pAppAttribute);
int TestAppAndDel(struct UKey *pUKey, struct AppAttribute *pAppAttribute);
int CreateCont(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute);
int OpenCont(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute);
int CloseCont(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute);
int LogIn(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct PinAttribute *pPinAttribute);
int ChangePin(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct PinAttribute *pOldPinAttribute, 
			struct PinAttribute *pNewPinAttribute);
int UnlockPin(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct PinAttribute *pUserPinAttribute,
			struct PinAttribute *pAdminPinAttribute);
int CreateFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileAttribute *pFileAttribute);
int ListFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			char *pFileName);
int ReadFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileData *pFileData);
int WriteFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileData *pFileData);
int DelFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileData *pFileData);

int GetCertificate(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct CertAttribute *pCertAttribute);
int SetCertificate(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct CertAttribute *pCertAttribute);


int GenKeyPair(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute);
int GetPubKey(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute);
int DelKey(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute);



int PubKeyEncrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData);
int PriKeyDecrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData);
int PubKeyDecrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData);
int PriKeyEncrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData);

int SymEncrypt(struct UKey *pUKey, unsigned int AlgMode, unsigned char *pInitVec,
			struct AppAttribute *pAppAttribute,	struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData);

int SymDecrypt(struct UKey *pUKey, unsigned int AlgMode, unsigned char *pInitVec,
			struct AppAttribute *pAppAttribute, struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData);


int ImportKey(struct UKey *pUKey, unsigned int AlgMode, unsigned char *pInitVec,
			struct AppAttribute *pAppAttribute,	struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute);

int InitialUKey(struct UKey *pUKey, struct AppAttribute *pAppAttribute,	
			struct ContAttribute *pContAttribute);

unsigned char * Hash(struct UKey *pUKey, unsigned char HashType, 
						struct HashData *pHashData);




#define DEBUG 1

#if DEBUG==1
#define PRINT(format, ...) printf("[%s][-%d-] "format"",__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define PRINT(format, ...)
#endif

#define LOG(format, ...) printf("[%s][-%d-] "format"",__FUNCTION__,__LINE__,##__VA_ARGS__)


#endif /*SCSIEXE_H_*/
