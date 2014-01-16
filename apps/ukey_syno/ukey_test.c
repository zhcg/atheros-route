#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <rpc/des_crypt.h>

#include "ukey_syno.h"

#include <crypt.h>

/***************************************************************************
 * name: init_io_hdr
 * parameter:
 * function: initialize the sg_io_hdr struct fields with the most common
 * 			 value
 * **************************************************************************/
struct  sg_io_hdr * init_io_hdr() {

	struct sg_io_hdr * p_scsi_hdr = (struct sg_io_hdr *)malloc(sizeof(struct sg_io_hdr));
	memset(p_scsi_hdr, 0, sizeof(struct sg_io_hdr));
	if (p_scsi_hdr) {
		p_scsi_hdr->interface_id = 'S'; /* this is the only choice we have! */
		p_scsi_hdr->flags = SG_FLAG_LUN_INHIBIT; /* this would put the LUN to 2nd byte of cdb*/
	}

	return p_scsi_hdr;
}

void destroy_io_hdr(struct sg_io_hdr * p_hdr) {
	if (p_hdr) {
		free(p_hdr);
	}
}

struct  UKey * init_UKey() {
	struct UKey *pUKey = (struct UKey *)malloc(sizeof(struct UKey));
	memset(pUKey, 0, sizeof(struct UKey));
	return pUKey;
}

void destroy_UKey(struct UKey * pUKey) {
	if (pUKey) {
		free(pUKey);
	}
}


//struct UKey *pUKey;

unsigned char sense_buffer[SENSE_LEN];
unsigned char data_buffer[BLOCK_LEN*256];

void show_hdr_outputs(struct sg_io_hdr * hdr) {
	printf("status:%d\n", hdr->status);
	printf("masked_status:%d\n", hdr->masked_status);
	printf("msg_status:%d\n", hdr->msg_status);
	printf("sb_len_wr:%d\n", hdr->sb_len_wr);
	printf("host_status:%d\n", hdr->host_status);
	printf("driver_status:%d\n", hdr->driver_status);
	printf("resid:%d\n", hdr->resid);
	printf("duration:%d\n", hdr->duration);
	printf("info:%d\n", hdr->info);
}

void show_sense_buffer(struct sg_io_hdr * hdr) {
	unsigned char * buffer = hdr->sbp;
	int i;
	for (i=0; i<hdr->mx_sb_len; ++i) {
		putchar(buffer[i]);
	}
}

void show_vendor(struct sg_io_hdr * hdr) {
	unsigned char * buffer = hdr->dxferp;
	int i;
	printf("vendor id:");
	for (i=8; i<16; ++i) {
		putchar(buffer[i]);
	}
	putchar('\n');
}

void show_product(struct sg_io_hdr * hdr) {
	unsigned char * buffer = hdr->dxferp;
	int i;
	printf("product id:");
	for (i=16; i<32; ++i) {
		putchar(buffer[i]);
	}
	putchar('\n');
}

void show_product_rev(struct sg_io_hdr * hdr) {
	unsigned char * buffer = hdr->dxferp;
	int i;
	printf("product ver:");
	for (i=32; i<36; ++i) {
		putchar(buffer[i]);
	}
	putchar('\n');
}



int main(int argc, char * argv[]) {
//	test_execute_Inquiry(argv[1], 0, 0);
	int i;
		struct UKey *pUKey = init_UKey();
	pUKey->p_hdr = init_io_hdr();

	pUKey->fd = open(argv[1], O_RDWR);
	LOG(" open sg file fd = 0x%x \n" ,pUKey->fd);
	if (pUKey->fd < 0){
		LOG("failed to open sg file \n");
		close(pUKey->fd);
		destroy_io_hdr(pUKey->p_hdr);
		destroy_UKey(pUKey);
		exit(1);
	}
#if 0
	struct AppData *localAppData = (struct AppData *)malloc(sizeof(struct AppData));
	memset(localAppData, 0, sizeof(struct AppData));
	LOG("Test Over %p!\n",localAppData);
#endif
	struct AppAttribute *localAppAttribute = (struct AppAttribute *)malloc(sizeof(struct AppAttribute));
	memset(localAppAttribute, 0, sizeof(struct AppAttribute));
	LOG("Test Over %p!\n",localAppAttribute);

	struct ContAttribute *localContAttribute = (struct ContAttribute *)malloc(sizeof(struct ContAttribute));
	memset(localContAttribute, 0, sizeof(struct ContAttribute));
	LOG("localContAttribute %p!\n",localContAttribute);
	
	struct PinAttribute *localPinAttribute = (struct PinAttribute *)malloc(sizeof(struct PinAttribute));
	memset(localPinAttribute, 0, sizeof(struct PinAttribute));

	struct KeyAttribute *localKeyAttribute = (struct KeyAttribute *)malloc(sizeof(struct KeyAttribute));
	memset(localKeyAttribute, 0, sizeof(struct KeyAttribute));
	
		
	struct CertAttribute *localCertAttribute = (struct CertAttribute *)malloc(sizeof(struct CertAttribute));
	memset(localCertAttribute, 0, sizeof(struct CertAttribute));

	int ret;
	unsigned char localRandom[8];
	unsigned char localUSBData[8];
//#if 0	
	//UsbOpen
	ret = UsbOpen(pUKey, localRandom, localUSBData);
	if(ret != 0)  LOG("UsbOpen Error\n");
	else LOG("UsbOpen  Succes!\n");
#if 1
	//InitialUkey
	ret = InitialUKey(pUKey, localAppAttribute, localContAttribute);
	if(ret != 0)  LOG("InitialUKey Error\n");
	else LOG("InitialUKey  Succes!\n");
#endif
	//GenRandom
	ret = GenRandom(pUKey, localRandom);
	if(ret != 0)  LOG("GenRandom Error\n");
	else LOG("GenRandom  Succes!\n");
	//GetKeyParam
//	ret = GetKeyParam(pUKey, random);
//	if(ret != 0)  LOG("GetKeyParam Error\n");

	//OpenApp
	const char app_name[] = {'c','a','_','a','p','p'};
	localAppAttribute->pAppName = (char *)app_name;
	localAppAttribute->AppNameLen = sizeof(app_name);
	ret = OpenApp(pUKey, localAppAttribute);
	if(ret != 0)  LOG("OpenApp Error\n");
	else LOG("OpenApp  Succes!\n");
	LOG("APPID = 0x%x\n", localAppAttribute->AppID);

	//CreateCont
	char ContName[] = "key_con1";
	localContAttribute->pContName = ContName;
	localContAttribute->ContNameLen = sizeof(ContName) - 1;
#if 1
	ret = CreateCont(pUKey, localAppAttribute, localContAttribute);
	if(ret != 0)  LOG("CreateCont Error\n");
	else LOG("CreateCont  Succes!\n");
	LOG("CreateCont ContID = 0x%x\n", localContAttribute->ContID);
#endif
	//OpenCont()
	ret = OpenCont(pUKey, localAppAttribute, localContAttribute);
	if(ret != 0)  LOG("OpenCont Error\n");
	else LOG("OpenCont  Succes!\n");
	LOG("OpenCont ContID = 0x%x\n", localContAttribute->ContID);

///////////////////////////////登陆////////////////////////////////////////////////
	char UserPin[] = "222222";
	char AdminPin[] = "111111";
	char NewPin[] = "333333";
	
	#if 1
	//LogIn(admin)
	memset(localPinAttribute, 0, sizeof(struct PinAttribute));
	localPinAttribute->PinLen = 8;
	memcpy(localPinAttribute->Pin, AdminPin, sizeof(AdminPin) - 1); 
	localPinAttribute->PinType = 0x00;
	ret = LogIn(pUKey, localAppAttribute, localPinAttribute);
	if(ret != 0)  LOG("LogIn admin Error\n");
	else LOG("LogIn admin  Succes!\n");
#endif
	//LogIn(user)
	localPinAttribute->PinLen = 8;
	memcpy(localPinAttribute->Pin, UserPin, sizeof(UserPin) - 1);	
	localPinAttribute->PinType = 0x01;
	ret = LogIn(pUKey, localAppAttribute, localPinAttribute);
	if(ret != 0)  LOG("LogIn user Error\n");
	else LOG("LogIn user  Succes!\n");

	//ChangePin
	struct PinAttribute *pChgPinAttribute = (struct PinAttribute *)malloc(sizeof(struct PinAttribute));
	memset(pChgPinAttribute, 0, sizeof(struct PinAttribute));
	pChgPinAttribute->PinLen = 16;
	memcpy(pChgPinAttribute->Pin, NewPin, sizeof(NewPin) - 1);
	pChgPinAttribute->PinType = 0x01;
	ret = ChangePin(pUKey, localAppAttribute, localPinAttribute, pChgPinAttribute);
	if(ret != 0)  LOG("ChangePin Error\n");
	else LOG("ChangePin Succes!\n");

		//LogIn(user)
		localPinAttribute->PinLen = 8;
		memcpy(localPinAttribute->Pin, NewPin, sizeof(NewPin) - 1);	
		localPinAttribute->PinType = 0x01;
		ret = LogIn(pUKey, localAppAttribute, localPinAttribute);
		if(ret != 0)  LOG("LogIn user Error\n");
		else LOG("LogIn user Succes!\n");
	int k;
	for(k=0;k<15;k++){
		//LogIn(user)
		localPinAttribute->PinLen = 8;
		memcpy(localPinAttribute->Pin, UserPin, sizeof(UserPin) - 1);	
		localPinAttribute->PinType = 0x01;
		ret = LogIn(pUKey, localAppAttribute, localPinAttribute);
		if(ret != 0)  LOG("LogIn user Error\n");
		else LOG("LogIn user  Succes!\n");

	}
	//LogIn(user)
	localPinAttribute->PinLen = 8;
	memcpy(localPinAttribute->Pin, NewPin, sizeof(NewPin) - 1); 
	localPinAttribute->PinType = 0x01;
	ret = LogIn(pUKey, localAppAttribute, localPinAttribute);
	if(ret != 0)  LOG("LogIn user Error\n");
	else LOG("LogIn user  Succes!\n");

	//UnlockPin
	memset(localPinAttribute, 0, sizeof(struct PinAttribute));
	localPinAttribute->PinLen = 8;
	memcpy(localPinAttribute->Pin, NewPin, sizeof(NewPin) - 1); 
	localPinAttribute->PinType = 0x01;
	
	memset(pChgPinAttribute, 0, sizeof(struct PinAttribute));
	pChgPinAttribute->PinLen = 8;
	memcpy(pChgPinAttribute->Pin, AdminPin, sizeof(AdminPin) - 1);
	pChgPinAttribute->PinType = 0x00;
	ret = UnlockPin(pUKey, localAppAttribute, localPinAttribute, pChgPinAttribute);
	if(ret != 0)  LOG("UnlockPin Error\n");
	else LOG("UnlockPin Succes!\n");


	//LogIn(user)
	memset(localPinAttribute, 0, sizeof(struct PinAttribute));
	localPinAttribute->PinLen = 8;
	memcpy(localPinAttribute->Pin, NewPin, sizeof(NewPin) - 1); 
	localPinAttribute->PinType = 0x01;
	ret = LogIn(pUKey, localAppAttribute, localPinAttribute);
	if(ret != 0)  LOG("LogIn user Error\n");
	else LOG("LogIn user  Succes!\n");

//////////////////////////////////文件操作/////////////////////////////////////////////

	//CreateFile(test)
	struct FileAttribute *localFileAttribute = (struct FileAttribute *)malloc(sizeof(struct FileAttribute));
	memset(localFileAttribute, 0, sizeof(struct FileAttribute));
	char Filename[] = "test";
//	char Filename[] = "CRight";
	memcpy(localFileAttribute->FileName, Filename, sizeof(Filename) - 1);
	localFileAttribute->FileSize = 0x400;
	localFileAttribute->ReadRights= 0xff;
	localFileAttribute->WriteRights= 0xff;	
	ret = CreateFile(pUKey, localAppAttribute, localFileAttribute);
	if(ret != 0)  LOG("CreateFile test Error\n");
	else LOG("CreateFile test Succes!\n");
	//CreateFile(try)
	memset(localFileAttribute, 0, sizeof(struct FileAttribute));
	char Filename1[] = "try";
	memcpy(localFileAttribute->FileName, Filename1, sizeof(Filename1) - 1);
	localFileAttribute->FileSize = 0x04;
	localFileAttribute->ReadRights= 0x10;
	localFileAttribute->WriteRights= 0x10;	
	ret = CreateFile(pUKey, localAppAttribute, localFileAttribute);
	if(ret != 0)  LOG("CreateFile try Error\n");
	else LOG("CreateFile try Succes!\n");

	//WriteFile()
	char WriteData[] = {0x74,0x65,0x73,0x74,0x27,0x73,0x20,0x64,0x61,0x74,0x61};
	struct FileData *localFileData = (struct FileData *)malloc(sizeof(struct FileData));
	memset(localFileData, 0, sizeof(struct FileData));
	localFileData->FileNameLen = sizeof(Filename) - 1;
	localFileData->pFileName = Filename;
	localFileData->DataLength = sizeof(WriteData);
	localFileData->pDataAddr = (unsigned char *)WriteData;
	ret = WriteFile(pUKey, localAppAttribute, localFileData);
	if(ret != 0)  LOG("WriteFile Error\n");
	else LOG("WriteFile Succes!\n");
	//ReadFile()
	localFileData->DataLength = 0x0b;
	char ReadData[localFileData->DataLength];
	localFileData->pDataAddr = (unsigned char *)ReadData;
	ret = ReadFile(pUKey, localAppAttribute, localFileData);
	if(ret != 0)  LOG("ReadFile Error\n");
	else LOG("ReadFile Succes!\n");
	//ListFile()
	char FileInfo[1024];
	ret = ListFile(pUKey, localAppAttribute, FileInfo);
	if(ret != 0)  LOG("ListFile Error\n");
	else LOG("ListFile Succes!\n");
#if 0
	//DelFile()
	ret = DelFile(pUKey, localAppAttribute, localFileData);
	if(ret != 0)  LOG("DelFile Error\n");
	else LOG("DelFile Succes!\n");
#endif

/////////////////////////////非对称加解密//////////////////////////////////////////////////


	struct OptData *localOptData = (struct OptData *)malloc(sizeof(struct OptData));
	memset(localOptData, 0, sizeof(struct OptData));
	struct BlockCipherParam localBlockCipherParam;

		//GenKeyPair
		localKeyAttribute->KeyLen = 0x400;
		//localKeyAttribute->KeyLen = 0x80;
		unsigned char localKey[localKeyAttribute->KeyLen];
		localKeyAttribute->KeyUse = 0x00;
		localKeyAttribute->pKey = localKey;
#if 1
		ret = GenKeyPair(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
		if(ret != 0)  LOG("GenKeyPair Error\n");
		else LOG("GenKeyPair Succes!\n");
		for(i=0; i<(localKeyAttribute->KeyLen / 8); i++)
			printf("%x ", localKey[i]);
		printf("\n");

		//GetPubKey
		memset(localKey, 0, sizeof(localKey));
		ret = GetPubKey(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
		if(ret != 0)  LOG("GetPubKey Error\n");
		else LOG("GetPubKey Succes!\n");
		for(i=0; i<(localKeyAttribute->KeyLen / 8); i++)
			printf("%x ", localKey[i]);
		printf("\n");




	//PubKeyEncrypt
	unsigned char RSAPlainData[] = {0x10,0x11 ,0x12,0x13,0x14,0x15 ,0x06,0x07,0x08,0x09};
	localOptData->PlainDataLen = sizeof(RSAPlainData);
	localOptData->pPlainData = RSAPlainData;
	unsigned char RSACipherData[128];
	localOptData->CipherDataLen = sizeof(RSACipherData);
	localOptData->pCipherData = RSACipherData;
#if 0

	ret = PubKeyEncrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PubKeyEncrypt Error\n");
	else LOG("PubKeyEncrypt Succes!\n");
	for(i=0; i<localOptData->CipherDataLen; i++)
		printf("%x ", RSACipherData[i]);
	printf("\n");
    localKeyAttribute->pKey = localOptData->pCipherData;



	//PriKeyDecrypt
	memset(RSAPlainData, 0, sizeof(RSAPlainData));
	ret = PriKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PriKeyDecrypt Error\n");
	else LOG("PriKeyDecrypt Succes!\n");
	for(i=0; i<localOptData->PlainDataLen; i++)
		printf("%x ", RSAPlainData[i]);
	printf("\n");

	localKeyAttribute->KeyLen = 0x80;

	ret = ImportKey(pUKey, SGD_SMS4_ECB, (unsigned char *)&localBlockCipherParam, localAppAttribute, localContAttribute, localKeyAttribute);
	if(ret != 0)  LOG("ImportKey Error\n");
	else LOG("ImportKey Succes!\n");

	//SymEncrypt
//	unsigned char SymKey[] ={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
//	localKeyAttribute->pKey= SymKey;
//	localKeyAttribute->KeyLen = sizeof(SymKey);
	unsigned char localPlainData[] = {0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38};
	localOptData->PlainDataLen = sizeof(localPlainData);
	localOptData->pPlainData = localPlainData;
	unsigned char localCipherData[16];
	localOptData->CipherDataLen = sizeof(localCipherData);
	localOptData->pCipherData = localCipherData;
	
	ret = SymEncrypt(pUKey, SGD_SMS4_ECB, (unsigned char *)&localBlockCipherParam, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("SymEncrypt Error\n");
	else LOG("SymEncrypt Succes!\n");
	for(i=0; i<localOptData->CipherDataLen; i++)
		printf("%x ", localCipherData[i]);
	printf("\n");
	//SymDecrypt
	memset(localPlainData, 0, sizeof(localPlainData));
	ret = SymDecrypt(pUKey, SGD_SMS4_ECB, (unsigned char *)&localBlockCipherParam, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("SymDecrypt Error\n");
	else LOG("SymDecrypt Succes!\n");
	for(i=0; i<localOptData->PlainDataLen; i++)
		printf("%x ", localPlainData[i]);
	printf("\n");

#endif	
	//PubKeyEncrypt
//	unsigned char RSAPlainData[] = {0x00,0x01 ,0x02,0x03,0x04,0x05 ,0x06,0x07,0x08,0x09 ,0x0a,0x0b,0x0c,0x0d ,0x0e,0x0f,0x10,0x11 ,0x12,0x13};
	localOptData->PlainDataLen = sizeof(RSAPlainData);
	localOptData->pPlainData = RSAPlainData;
//	unsigned char RSACipherData[128];
	localOptData->CipherDataLen = sizeof(RSACipherData);
	localOptData->pCipherData = RSACipherData;
	ret = PubKeyEncrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PubKeyEncrypt Error\n");
	else LOG("PubKeyEncrypt Succes!\n");
	for(i=0; i<localOptData->CipherDataLen; i++)
		printf("%x ", RSACipherData[i]);
	printf("\n");
	
	//PriKeyDecrypt
	memset(RSAPlainData, 0, sizeof(RSAPlainData));
	ret = PriKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PriKeyDecrypt Error\n");
	else LOG("PriKeyDecrypt Succes!\n");
	for(i=0; i<localOptData->PlainDataLen; i++)
		printf("%x ", RSAPlainData[i]);
	printf("\n");
#endif
	unsigned char RSAPlainData1[] = {0x10,0x11 ,0x12,0x13,0x14,0x15 ,0x06,0x07,0x08,0x09};
	localOptData->PlainDataLen = sizeof(RSAPlainData1);
	localOptData->pPlainData = RSAPlainData1;
	unsigned char RSACipherData1[132];
	localOptData->CipherDataLen = sizeof(RSACipherData1);
	localOptData->pCipherData = RSACipherData1;
		/////////////////////////////证书操作//////////////////////////////////////////////////
#if 0	
			//SetCertificate
			localCertAttribute->CertLen = 0x80;
			unsigned char localCert[localCertAttribute->CertLen];
			memset(localCert, 0, sizeof(localCert));
			localCertAttribute->pCert = localCert;
			localCertAttribute->CertUse = 0x01;
			ret = SetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("SetCertificate Error\n");
			else LOG("SetCertificate Succes!\n");
			//GetCertificate
			ret = GetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("GetCertificate Error\n");
			else LOG("GetCertificate Succes!\n");
#endif


/////////////////////////////签名//////////////////////////////////////////////////

	//GenKeyPair
			localKeyAttribute->KeyLen = 0x400;
			//localKeyAttribute->KeyLen = 0x80;
//			unsigned char localKey[localKeyAttribute->KeyLen];
			localKeyAttribute->KeyUse = 0x01;
			localKeyAttribute->pKey = localKey;
			ret = GenKeyPair(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
			if(ret != 0)  LOG("GenKeyPair Error\n");
			else LOG("GenKeyPair Succes!\n");
			for(i=0; i<(localKeyAttribute->KeyLen / 8); i++)
				printf("%x ", localKey[i]);
			printf("\n");
	
			//GetPubKey
			memset(localKey, 0, sizeof(localKey));
			ret = GetPubKey(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
			if(ret != 0)  LOG("GetPubKey Error\n");
			else LOG("GetPubKey Succes!\n");
			for(i=0; i<(localKeyAttribute->KeyLen / 8); i++)
				printf("%x ", localKey[i]);
			printf("\n");



	//PriKeyEncrypt	
	ret = PriKeyEncrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PriKeyEncrypt Error\n");
	else LOG("PriKeyEncrypt Succes!\n");
	for(i=0; i<localOptData->CipherDataLen; i++)
		printf("%x ", RSACipherData1[i]);
	printf("\n");

	//PubKeyDecrypt
	memset(RSAPlainData1, 0, sizeof(RSAPlainData1));
	ret = PubKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PubKeyDecrypt Error\n");
	else LOG("PubKeyDecrypt Succes!\n");
	for(i=0; i<132; i++)
		printf("%x ", RSAPlainData1[i]);
	printf("\n");
	//外部公钥解密

	localOptData->PlainDataLen = sizeof(RSACipherData1);
	localOptData->pPlainData = RSACipherData1;

	localOptData->CipherDataLen = 132;
	localOptData->pCipherData = RSAPlainData1;
	//外部公钥加密
	RSAPlainData1[5] = 0x02;
	ret = PubKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PubKeyDecrypt Error\n");
	else LOG("PubKeyDecrypt Succes----------------------!\n");
	for(i=0; i<localOptData->CipherDataLen; i++)
		printf("%x ", RSACipherData1[i]);
	printf("\n");
	//私钥解密
	localOptData->CipherDataLen = 128;
	localOptData->pCipherData = RSACipherData1 + 4;
//	memset(RSAPlainData, 0, sizeof(RSAPlainData));
	ret = PriKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
	if(ret != 0)  LOG("PriKeyDecrypt Error\n");
	else LOG("PriKeyDecrypt Succes!\n");
	for(i=0; i<localOptData->PlainDataLen; i++)
		printf("%x ", RSAPlainData1[i]);
	printf("\n");


/////////////////////////////证书操作//////////////////////////////////////////////////
#if 0	
		//SetCertificate
		localCertAttribute->CertLen = 0x80;
//		unsigned char localCert[localCertAttribute->CertLen];
		memset(localCert, 0, sizeof(localCert));
		localCertAttribute->pCert = localCert;
		localCertAttribute->CertUse = 0x01;
		ret = SetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
		if(ret != 0)  LOG("SetCertificate Error\n");
		else LOG("SetCertificate Succes!\n");
		//GetCertificate
		ret = GetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
		if(ret != 0)  LOG("GetCertificate Error\n");
		else LOG("GetCertificate Succes!\n");
#endif

///////////////////////////////////////hash////////////////////////////////////////////////////////////////////////

	struct HashData *localHashData = (struct HashData *)malloc(sizeof(struct HashData));
	memset(localHashData, 0, sizeof(struct HashData));

	
	unsigned char HashDataIn[] = {'h','e','l','l','o'};
	unsigned char *HashDataOut;
	localHashData->data = HashDataIn;
	localHashData->len = 5;

#if 0
	HashDataOut = Hash(pUKey, 1, localHashData);
	printf("HashDataOut :::\n");
	for(i=0; i<100; i++)
		printf("%x ", HashDataOut[i]);
	printf("\n");
#endif

	HashDataOut = Hash(pUKey, 2, localHashData);
	printf("HashDataOut :::\n");
	for(i=0; i<20; i++)
		printf("%x ", HashDataOut[i]);
	printf("\n");


#if 1
#define MAXLINE 4096

   int    listenfd, connfd;
    struct sockaddr_in     servaddr;
    unsigned char    buff[4096];
    int     n;
	
    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
    printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
    exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(6666);

    if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
    printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
    exit(0);
    }

    if( listen(listenfd, 10) == -1){
    printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
    exit(0);
    }

    printf("======waiting for client's request======\n");
    while(1){
#if 1
    if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
        printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
        continue;
   }
#endif
#if 0	
  	  n = recv(connfd, buff, MAXLINE, 0);
  	  buff[n] = '\0';
      printf("recv msg from client: %s\n", buff);
#else
	n = recv(connfd, buff, MAXLINE, 0);
	for(i=0; i<n; i++)
		printf("%x ", buff[i]);
	printf("\n");

	unsigned char IpPackCmd = buff[0];
	unsigned short IpPackLen = ((buff[1] << 8) + buff[2]) & 0xffff;
	printf("receive localIpPack->len %x :::::::::::%x\n",n ,IpPackLen);

	switch(IpPackCmd){

		case 1: {//ChgPubKey sign 
//			memcpy(localKey, (buff + 3), IpPackLen);
			printf("receive pubkey::::::sign:::::\n");
			for(i=0; i<IpPackLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");
				//SetCertificate
			localCertAttribute->CertLen = IpPackLen;
			localCertAttribute->pCert = (buff + 3);
			localKeyAttribute->KeyUse= 0x01;
			ret = SetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("SetCertificate Error\n");
			else LOG("SetCertificate Succes!\n");
	#if 0
				//GetCertificate
			localCertAttribute->CertLen = IpPackLen + 2;
			ret = GetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("GetCertificate Error\n");
			else LOG("GetCertificate Succes!\n");
			printf("GetCertificate === pubkey  ?????\n");
			for(i=0; i<localCertAttribute->CertLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");
	#endif
				//GenKeyPair
			localKeyAttribute->KeyLen = 0x400;
			localKeyAttribute->pKey = (buff + 3);
			ret = GenKeyPair(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
			if(ret != 0)  LOG("GenKeyPair Error\n");
			else LOG("GenKeyPair Succes!\n");

				//GetPubKey
			
			ret = GetPubKey(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
			if(ret != 0)  LOG("GetPubKey Error\n");
			else LOG("GetPubKey Succes!\n");

			printf("GetPubKey :::::::::::\n");
			for(i=0; i<IpPackLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");

	//		memcpy((buff + 3), localKey, IpPackLen);
			
			if(send(connfd, buff, (IpPackLen + 3), 0) < 0)
   	 		{
    			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
    		//	exit(0);
    		}

			break;}
		case 2: {//ChgPubKey encrypt
//			memcpy(localKey, (buff + 3), IpPackLen);
			printf("receive pubkey::::::encrypt:::::\n");
			for(i=0; i<IpPackLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");
				//SetCertificate
			localCertAttribute->CertLen = IpPackLen;
			localCertAttribute->pCert = (buff + 3);
			localKeyAttribute->KeyUse= 0x00;
			ret = SetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("SetCertificate Error\n");
			else LOG("SetCertificate Succes!\n");
	#if 0
				//GetCertificate
			localCertAttribute->CertLen = IpPackLen + 2;
			ret = GetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("GetCertificate Error\n");
			else LOG("GetCertificate Succes!\n");
			printf("GetCertificate === pubkey  ?????\n");
			for(i=0; i<localCertAttribute->CertLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");
	#endif
				//GenKeyPair
			localKeyAttribute->KeyLen = 0x400;
			localKeyAttribute->pKey = (buff + 3);
			ret = GenKeyPair(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
			if(ret != 0)  LOG("GenKeyPair Error\n");
			else LOG("GenKeyPair Succes!\n");

				//GetPubKey
			
			ret = GetPubKey(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
			if(ret != 0)  LOG("GetPubKey Error\n");
			else LOG("GetPubKey Succes!\n");

			printf("GetPubKey :::::::::::\n");
			for(i=0; i<IpPackLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");

	//		memcpy((buff + 3), localKey, IpPackLen);
			
			if(send(connfd, buff, (IpPackLen + 3), 0) < 0)
   	 		{
    			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
    		//	exit(0);
    		}

			break;		}

		case 3: {//sign
//			memcpy(localKey, (buff + 3), IpPackLen);
			printf("receive sign:::::::::::\n");
			for(i=0; i<IpPackLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");
			//获得外部公钥
			unsigned char 	FarPubKey[200];
			localCertAttribute->CertLen = 136;
			localCertAttribute->pCert = FarPubKey;
			localKeyAttribute->KeyUse= 0x01;
			ret = GetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("GetCertificate Error\n");
			else LOG("GetCertificate Succes!\n");
			for(i=0; i<136; i++)
				printf("%x ", FarPubKey[i]);
			printf("\n");
			//使用外部公钥验签	
			unsigned char 	RSAPlainData[132];
			localOptData->PlainDataLen = sizeof(RSAPlainData);
			localOptData->pPlainData = RSAPlainData;

			localOptData->CipherDataLen = IpPackLen;
			localOptData->pCipherData = (buff + 3);
			localKeyAttribute->pKey = FarPubKey + 2;
			
			ret = PubKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
			if(ret != 0)  LOG("PubKeyDecrypt Error\n");
			else LOG("PubKeyDecrypt Succes!\n");
			for(i=0; i<132; i++)
				printf("%x ", RSAPlainData[i]);
			printf("\n");			

			//生成签名
			for(i=122; i<132; i++)
				RSAPlainData[i] += 10;
			localOptData->PlainDataLen = 10;
			localOptData->pPlainData = (RSAPlainData + 122);			
			localOptData->CipherDataLen = 132;
			localOptData->pCipherData = (buff + 3);
			IpPackLen = localOptData->pCipherData;

			ret = PriKeyEncrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
			if(ret != 0)  LOG("PriKeyEncrypt Error\n");
			else LOG("PriKeyEncrypt Succes!\n");
			for(i=0; i<localOptData->CipherDataLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");			
			if(send(connfd, buff, (IpPackLen + 3), 0) < 0)
   	 		{
    			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
    		//	exit(0);
    		}
			break;}


		case 4: {//公钥加密、私钥解密
//			memcpy(localKey, (buff + 3), IpPackLen);
			printf("receive PubKeyEncrypt :::::::::::\n");
			for(i=0; i<IpPackLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");
			//私钥解密
			localKeyAttribute->KeyUse= 0x00;

			localOptData->PlainDataLen = sizeof(RSAPlainData);
			localOptData->pPlainData = RSAPlainData;

			localOptData->CipherDataLen = (IpPackLen - 4);
			localOptData->pCipherData = (buff + 7);
			
			ret = PriKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
			if(ret != 0)  LOG("PriKeyDecrypt Error\n");
			else LOG("PriKeyDecrypt Succes!\n");

			unsigned short PlainDataLen = (RSAPlainData[0] << 8) + RSAPlainData[1];
			unsigned char tempData[PlainDataLen];
			for(i=0; i<PlainDataLen; i++)
				printf("%x ", RSAPlainData[i]);
			printf("\n");

			memcpy(tempData, RSAPlainData, PlainDataLen);
			
			//获得外部公钥
			localKeyAttribute->KeyUse= 0x01;
			unsigned char 	FarPubKey[200];
			localCertAttribute->CertLen = 136;
			localCertAttribute->pCert = FarPubKey;
			ret = GetCertificate(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localCertAttribute);
			if(ret != 0)  LOG("GetCertificate Error\n");
			else LOG("GetCertificate Succes!\n");
			for(i=0; i<136; i++)
				printf("%x ", FarPubKey[i]);
			printf("\n");
			//使用外部公钥加密
		//	unsigned char 	RSAPlainData[132];
			RSAPlainData[0] = 0x80;
			RSAPlainData[1] = 0x00;
			RSAPlainData[2] = 0x00;
			RSAPlainData[3] = 0x00;

			RSAPlainData[4] = 0x00;
			RSAPlainData[5] = 0x02;
			for (i = 0; i < 125 - PlainDataLen; i++) 
			{
				RSAPlainData[i + 6] = 0xFF;
			}
			RSAPlainData[131 - PlainDataLen] = 0x00;
			memcpy((RSAPlainData + 132 - PlainDataLen), tempData, PlainDataLen);
			printf("far PubKeyEncrypt input:::::::::::\n");
			for(i=0; i<132; i++)
				printf("%x ", RSAPlainData[i]);
			printf("\n");
		
			localOptData->CipherDataLen = 132;
			localOptData->pCipherData = RSAPlainData;

			localOptData->PlainDataLen = 132;
			localOptData->pPlainData = (buff + 3);
			localKeyAttribute->pKey = FarPubKey + 2;
			
			ret = PubKeyDecrypt(pUKey, localAppAttribute, localContAttribute, localKeyAttribute, localOptData);
			if(ret != 0)  LOG("PubKeyDecrypt Error\n");
			else LOG("PubKeyDecrypt Succes-------waibugongyaojiami---------------!\n");
			for(i=0; i<localOptData->PlainDataLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");			
			if(send(connfd, buff, (IpPackLen + 3), 0) < 0)
   	 		{
    			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
    		//	exit(0);
    		}
			break;}


		case 5: {//HASH
//			memcpy(localKey, (buff + 3), IpPackLen);
			printf("HashDataIn :::::::::::\n");
			for(i=0; i<IpPackLen; i++)
				printf("%x ", buff[i + 3]);
			printf("\n");
			localHashData->data = (buff + 3);
			localHashData->len = IpPackLen;

			HashDataOut = Hash(pUKey, 2, localHashData);
			printf("HashDataOut :::::::::::\n");
			for(i=0; i<20; i++)
				printf("%x ", HashDataOut[i]);
			printf("\n");
			memcpy((buff + 3), HashDataOut, 20);
			IpPackLen = 20;
			buff[1] = (IpPackLen >> 8) & 0xff;
			buff[2] = IpPackLen & 0xff;
			if(send(connfd, buff, (IpPackLen + 3), 0) < 0)
   	 		{
    			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
    		//	exit(0);
    		}
			break;}
		default:
			printf("no cmd!!!!!!!!!!!!\n");

	}
#endif
	close(connfd);
   }

    close(listenfd);
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//DelKey
	ret =DelKey(pUKey, localAppAttribute, localContAttribute, localKeyAttribute);
	if(ret != 0)  LOG("DelKey Error\n");
	else LOG("DelKey Succes!\n");

	//CloseCont
	localContAttribute->pContName = ContName;
	localContAttribute->ContNameLen = sizeof(ContName) - 1;
	ret = CloseCont(pUKey, localAppAttribute, localContAttribute);
	if(ret != 0)  LOG("CloseCont Error\n");
	else LOG("CloseCont Succes!\n");

	//CloseApp
	ret = CloseApp(pUKey, localAppAttribute);
	if(ret != 0)  LOG("CloseApp Error\n");
	else LOG("CloseApp Succes!\n");
		
	close(pUKey->fd);
#if 1	
//	LOG("Test Over %p!\n",localAppData);
	free(localHashData);
	LOG("Test Over 1!\n");
	free(localAppAttribute);
	LOG("Test Over 2!\n");
	free(localContAttribute);
	LOG("Test Over 3!\n");
	free(localPinAttribute);
	LOG("Test Over 4!\n");
	free(pChgPinAttribute);
	LOG("Test Over 5!\n");
	free(localKeyAttribute);
	LOG("Test Over 6!\n");
	free(localCertAttribute);
	LOG("Test Over 7!\n");
	free(localOptData);
	LOG("Test Over 8!\n");
	free(pUKey->p_hdr);
	LOG("Test Over 9!\n");
	free(pUKey);
	LOG("Test Over!\n");
#endif
	return EXIT_SUCCESS;
}

