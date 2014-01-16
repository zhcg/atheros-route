#include <unistd.h>

#include <scsi/scsi_ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#include <crypt.h>

#include "ukey_syno.h"
#define Null  0 

#define DELAY usleep(100)
#define L2B16(X)            ((((unsigned short)(X) & 0xff00) >> 8) |(((unsigned short)(X) & 0x00ff) << 8)) 
#define L2B32(X)          ((((unsigned int)(X) & 0xff000000) >> 24) | \
		                   (((unsigned int)(X) & 0x00ff0000) >> 8) | \
					       (((unsigned int)(X) & 0x0000ff00) << 8) | \
					       (((unsigned int)(X) & 0x000000ff) << 24))

void get8(char *cp, char *datain)
{
	int i,j;

	for(i=0;i<8;i++){
//		scanf("%2x",&t);
//		if(feof(stdin))
//		  good_bye();
		for(j=0; j<8 ; j++) {
		  *cp++ = (*datain & (0x01 << (7-j))) != 0;
		}
		datain++;
	}
}
void put8(char *cp, char *dataout)
{
	int i,j,t;

	for(i=0;i<8;i++){
	  t = 0;
	  for(j = 0; j<8; j++)
	    t = (t<<1) | *cp++;
	  *dataout++ = t;
	}
}

void des_encrypt(char *key, char *data, int len)
{
    char pkey[64],plain[128];
    char i;
   	get8(pkey, key);
    setkey(pkey);

//    while (len % 8 != 0){
 //       data[++len] = '\x8';
 //   };
    for(i = 0; i < len; i = i + 8){
   	 	get8(plain, (data + i));
    	encrypt(plain, 0);
    	put8(plain, (data + i));
  	}
}

void des_decrypt(char *key, char *data, int len)
{
    char pkey[64],plain[128];
    char i;
   	get8(pkey, key);
    setkey(pkey);
		for(i = 0; i < len; i = i + 8){
   	 	get8(plain, (data + i));
    	encrypt(plain, 1);
    	put8(plain, (data + i));
  	}
}



/***************************************************************************
 * name: UsbOpen
 * parameter:
 * 		pUKey:	file descripter
 * 		pRandom:	cdb page code
 * function:
 * **************************************************************************/

int UsbOpen(struct UKey *pUKey, unsigned char *pRandom, unsigned char *pUsbData) {	
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x08;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x08;
	pUKey->localTPCCmd.LE = 0x08;	

	unsigned char wr_temp[8] = {0xd9,0x13,0xfc,0xce,0xa4,0xbe,0x16,0xc3};
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	pUKey->localTDataPack.DATA = pUsbData;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){		
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}
	
return ret;
}

/***************************************************************************
 * name: GenRandom
 * parameter:
 * 		pUKey:	file descripter
 * 		pRandom:	cdb page code
 * function:
 * **************************************************************************/

int GenRandom(struct UKey *pUKey, unsigned char *pRandom) {	
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x50;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x02;
	pUKey->localTPCCmd.LE = 0x08;	
	
	unsigned char wr_temp[2] = {0x00,0x00};
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x08;
	pUKey->localTDataPack.DATA = pRandom;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}
#if 0
/***************************************************************************
 * name: GetKeyParam
 * parameter:
 * 		pUKey:	file descripter
 * 		pRandom:	cdb page code
 * function:
 * **************************************************************************/

int GetKeyParam(struct UKey *pUKey, unsigned char *pKeyParam) {	
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x04;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x02;
	pUKey->localTPCCmd.LE = 0x11e;	
	
	unsigned char wr_temp[2] = {0x00,0x00};
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = 0x11e;
	pUKey->localTDataPack.DATA = pKeyParam;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
#if 0	
	if (pUKey->p_hdr->dxfer_len){
		LOG("Receive data %d \n",pUKey->p_hdr->dxfer_len);
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
	}
//#endif
//#if DEBUG
	int i;
	for (i=0;i<pUKey->localTPCCmd.LE;i++)
		PRINT("random[%d] = 0x%x \n" ,i ,pUKey->localTDataPack.DATA[i]);
#endif	

	return pUKey->p_hdr->status;
}
#endif
#if 0
/***************************************************************************
 * name: NewApp
 * parameter:
 * 		pUKey:	
 * 		pAppData:
 *		pAppID:
 * function:
 * **************************************************************************/
int NewApp(struct UKey *pUKey, struct AppData *pAppData, struct AppAttribute *pAppAttribute){	
		
	pUKey->localTDataPack.DATA = (unsigned char *)pAppData;

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x20;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x60;
	pUKey->localTPCCmd.LE = 0x0a;	
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = (unsigned char *)&pAppAttribute->AppID;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+10, 2);//\B5\DA11\A1\A212\D7ֽ\DAΪӦ\D3\C3ID
	}else{
		LOG("Receive data error\n");
		return -1;
	}	

	return ret;
}
#endif	
/***************************************************************************
 * name: OpenApp
 * parameter:
 * 		pUKey:	
 * 		pAppName:
 *		AppNameLen:
 *		pAppID:
 * function:
 * **************************************************************************/

int OpenApp(struct UKey *pUKey, struct AppAttribute *pAppAttribute){	
		
	pUKey->localTDataPack.DATA = (unsigned char *)pAppAttribute->pAppName;

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x26;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pAppAttribute->AppNameLen;
	pUKey->localTPCCmd.LE = 0x0a;	
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = (unsigned char *)&pAppAttribute->AppID;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		//memcpy(pUKey->localTDataPack.DATA, rd_temp+10, pUKey->localTPCCmd.LE);//\B5\DA11\A1\A212\D7ֽ\DAΪӦ\D3\C3ID
		pAppAttribute->AppID = (rd_temp[11] << 8) + rd_temp[10];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
	}
		
/***************************************************************************
 * name: CloseApp
 * parameter:
 * 		pUKey:	
 *		pAppID:
 * function:
 * **************************************************************************/

int CloseApp(struct UKey *pUKey, struct AppAttribute *pAppAttribute){	
		
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x28;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x02;
	pUKey->localTPCCmd.LE = 0x00;	
	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	pUKey->localTDataPack.DATA = wr_temp; 
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

return ret;
}

/***************************************************************************
 * name: CreateCont
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pContAttribute:
 * function:
 * **************************************************************************/
int CreateCont(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute){	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x40;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pContAttribute->ContNameLen + 2;
	pUKey->localTPCCmd.LE = 0x02;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = (pAppAttribute->AppID >> 8) & 0xff;
	memcpy(wr_temp + 2, pContAttribute->pContName, pContAttribute->ContNameLen);
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = (unsigned char *)&pContAttribute->ContID;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;

	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		LOG("ret = 0x%x\n", ret);
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}
	
	
/***************************************************************************
 * name: OpenCont
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pContAttribute:
 * function:
 * **************************************************************************/
int OpenCont(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute){	
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x42;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pContAttribute->ContNameLen + 2;
	pUKey->localTPCCmd.LE = 0x02;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	memcpy(wr_temp + 2, pContAttribute->pContName, pContAttribute->ContNameLen);
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	pUKey->localTDataPack.DATA = (unsigned char *)&pContAttribute->ContID;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	//	memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
		pContAttribute->ContID =  (rd_temp[3] << 8) + rd_temp[2];

	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}

	
	
/***************************************************************************
 * name: CloseCont
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pContAttribute:
 * function:
 * **************************************************************************/
int CloseCont(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x44;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x04;
	pUKey->localTPCCmd.LE = 0x00;	
	
	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
	pUKey->localTDataPack.DATA = wr_temp;
		
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	pUKey->localTDataPack.DATA = (unsigned char *)&pContAttribute->ContID;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
		
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;	
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	//	memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
		pContAttribute->ContID =  (rd_temp[3] << 8) + rd_temp[2];
	}else{
		LOG("Receive data error\n");
		return -1;
	}
	
	return ret;
}
	
/***************************************************************************
 * name: LogIn
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pPinAttribute:
 * function:
 * **************************************************************************/
int LogIn(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct PinAttribute *pPinAttribute){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x18;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = pPinAttribute->PinType;
	pUKey->localTPCCmd.LC = pPinAttribute->PinLen + 2;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	memcpy(wr_temp + 2, pPinAttribute->Pin, pPinAttribute->PinLen);
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;

	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}

	
	
/***************************************************************************
 * name: ChangePin
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pPinAttribute:
 * function:
 * **************************************************************************/
int ChangePin(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct PinAttribute *pOldPinAttribute, 
			struct PinAttribute *pNewPinAttribute){	
		
	int ret;
	int i;
	ret = LogIn(pUKey, pAppAttribute, pOldPinAttribute);
	if(ret != 0){
		LOG("LogIn return error\n");
		return -1;
	}
	//ʹ\D3\C3OldPin \BC\D3\C3\DC NewPin
	pNewPinAttribute->Pin[8] = 0x80;
	//ecb_crypt((char *)pOldPinAttribute->Pin, (char *)pNewPinAttribute->Pin, 16, DES_ENCRYPT);
	des_encrypt((char *)pOldPinAttribute->Pin, (char *)pNewPinAttribute->Pin, 16);
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x16;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = pNewPinAttribute->PinType;
	pUKey->localTPCCmd.LC = 0x12;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	for (i=0;i<16;i++)	
		wr_temp[i+2] = pNewPinAttribute->Pin[i];
	//memcpy(wr_temp + 2, pNewPinAttribute->Pin, pNewPinAttribute->PinLen);
	pUKey->localTDataPack.DATA = wr_temp;

	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}

	
	
/***************************************************************************
 * name: UnlockPin
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pPinAttribute:
 * function:
 * **************************************************************************/
int UnlockPin(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct PinAttribute *pUserPinAttribute,
			struct PinAttribute *pAdminPinAttribute){	
	int ret;
	int i;	
	ret = LogIn(pUKey, pAppAttribute, pAdminPinAttribute);
	if(ret != 0){
		LOG("LogIn return error\n");
		return -1;
	}
	//ʹ\D3\C3AdminPin \BC\D3\C3\DC UserPin
	pUserPinAttribute->Pin[8] = 0x80;
//ecb_crypt((char *)pUserPinAttribute->Pin, (char *)pAdminPinAttribute->Pin, 16, DES_ENCRYPT);
	des_encrypt((char *)pAdminPinAttribute->Pin,(char *)pUserPinAttribute->Pin, 16);
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x1a;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x12;
	pUKey->localTPCCmd.LE = 0x00;	
	
	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	for (i=0;i<16;i++)	
		wr_temp[i+2] = pUserPinAttribute->Pin[i];
	//memcpy(wr_temp + 2, pNewPinAttribute->Pin, pNewPinAttribute->PinLen);
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}
	
	return ret;

}


/***************************************************************************
 * name: CreateFile
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pContAttribute:
 * function:
 * **************************************************************************/
int CreateFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileAttribute *pFileAttribute){	
		
	pUKey->localTDataPack.DATA = (unsigned char *)pFileAttribute;

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x30;
	pUKey->localTPCCmd.P1 = pAppAttribute->AppID >> 8;
	pUKey->localTPCCmd.P2 = pAppAttribute->AppID & 0xff;
	pUKey->localTPCCmd.LC = sizeof(struct FileAttribute);
	pUKey->localTPCCmd.LE = 0x00;	
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	pFileAttribute->FileSize = L2B32(pFileAttribute->FileSize);
	pFileAttribute->ReadRights = L2B32(pFileAttribute->ReadRights);
	pFileAttribute->WriteRights = L2B32(pFileAttribute->WriteRights);

	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	pFileAttribute->FileSize = L2B32(pFileAttribute->FileSize);
	pFileAttribute->ReadRights = L2B32(pFileAttribute->ReadRights);
	pFileAttribute->WriteRights = L2B32(pFileAttribute->WriteRights);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}



/***************************************************************************
 * name: ListFile
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pContAttribute:
 * function:
 * **************************************************************************/
int ListFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			char *pFileName){	
		
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x34;
	pUKey->localTPCCmd.P1 = pAppAttribute->AppID >> 8;
	pUKey->localTPCCmd.P2 = pAppAttribute->AppID & 0xff;
	pUKey->localTPCCmd.LC = 0x02;
	pUKey->localTPCCmd.LE = 0x400;	
	
	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = 0x00;
	wr_temp[1] = 0x00;
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.DATA = (unsigned char *)pFileName;
	pUKey->localTDataPack.LC = 0x400;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}	

	return ret;
	}

/***************************************************************************
 * name: ReadFile
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pFileData:
 * function:
 * **************************************************************************/
int ReadFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileData *pFileData){	
		
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x38;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pFileData->FileNameLen + 0x06;
	pUKey->localTPCCmd.LE = pFileData->DataLength;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
#if 0
	memcpy(wr_temp + 2, pFileData->pFileName, pFileData->FileNameLen);
	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
#else
	wr_temp[2] = 0x00;
	wr_temp[3] = 0x00;
	wr_temp[4] = pFileData->FileNameLen & 0xff;
	wr_temp[5] = pFileData->FileNameLen >> 8;
	memcpy(wr_temp + 6, pFileData->pFileName, pFileData->FileNameLen);
#endif	
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = pFileData->pDataAddr;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}


/***************************************************************************
 * name: WriteFile
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pFileData:
 * function:
 * **************************************************************************/
int WriteFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileData *pFileData){	
		
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x3a;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pFileData->FileNameLen + 0x08 + pFileData->DataLength;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
#if 0
	memcpy(wr_temp + 2, pFileData->pFileName, pFileData->FileNameLen);
	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	memcpy((wr_temp + pFileData->FileNameLen + 11), &pFileData->pDataAddr, pFileData->DataLength);
#else	
	wr_temp[2] = 0x00;
	wr_temp[3] = 0x00;
	wr_temp[4] = pFileData->FileNameLen & 0xff;
	wr_temp[5] = pFileData->FileNameLen >> 8;
	memcpy(wr_temp + 6, pFileData->pFileName, pFileData->FileNameLen);
	wr_temp[pFileData->FileNameLen + 6] = pFileData->DataLength & 0xff;
	wr_temp[pFileData->FileNameLen + 7] = pFileData->DataLength >> 8;
	memcpy((wr_temp + pFileData->FileNameLen + 8), pFileData->pDataAddr, pFileData->DataLength);
#endif
	pUKey->localTDataPack.DATA = wr_temp;

	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	unsigned char rd_temp[pUKey->localTDataPack.LC  + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;	
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}


	return ret;
	}


/***************************************************************************
 * name: DelFile
 * parameter:
 * 		pUKey:	
 * 		pAppAttribute:
 *		pFileData:
 * function:
 * **************************************************************************/
int DelFile(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct FileData *pFileData){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x32;
#if 0
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pFileData->FileNameLen + 0x02;
#else
	pUKey->localTPCCmd.P1 = pAppAttribute->AppID >> 8;
	pUKey->localTPCCmd.P2 = pAppAttribute->AppID & 0xff;
	pUKey->localTPCCmd.LC = pFileData->FileNameLen;
#endif
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
#if 0
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;	
	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
#else
	memcpy(wr_temp, pFileData->pFileName, pFileData->FileNameLen);
#endif
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x80;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return pUKey->p_hdr->status;
}



/***************************************************************************
 * name: GetCertificate
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int GetCertificate(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct CertAttribute *pCertAttribute){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x4e;
	pUKey->localTPCCmd.P1 = pKeyAttribute->KeyUse;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x04;
	pUKey->localTPCCmd.LE = pCertAttribute->CertLen;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
	
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x100;
	pUKey->localTDataPack.DATA = pCertAttribute->pCert;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}	

	return ret;
}




/***************************************************************************
 * name: SetCertificate
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int SetCertificate(struct UKey *pUKey, struct AppAttribute *pAppAttribute, 
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct CertAttribute *pCertAttribute){	
		
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x4C;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pCertAttribute->CertLen + 0x09;
	pUKey->localTPCCmd.LE = pCertAttribute->CertLen;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
	wr_temp[4] = pKeyAttribute->KeyUse;
	wr_temp[5] = pCertAttribute->CertLen & 0xff;
	wr_temp[6] = (pCertAttribute->CertLen >> 8) & 0xff;
	wr_temp[7] = (pCertAttribute->CertLen >> 16) & 0xff;
	wr_temp[8] = (pCertAttribute->CertLen >> 24) & 0xff;
	memcpy(wr_temp + 9, pCertAttribute->pCert, pCertAttribute->CertLen);
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);

	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
//	pUKey->localTDataPack.DATA = pCertAttribute->pCert;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;

	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}

/***************************************************************************
 * name: GenKeyPair
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int GenKeyPair(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x54;
	pUKey->localTPCCmd.P1 = pKeyAttribute->KeyUse;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x06;
	pUKey->localTPCCmd.LE = 0x84;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
	wr_temp[4] = pKeyAttribute->KeyLen & 0xff;
	wr_temp[5] = pKeyAttribute->KeyLen >> 8;
//	memcpy(wr_temp + 2, pFileData->pFileName, pFileData->FileNameLen);
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = pKeyAttribute->pKey;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, (pKeyAttribute->KeyLen / 8));
	}else{
		LOG("Receive data error\n");
		return -1;
	}
	return ret;
}


/***************************************************************************
 * name: GetPubKey
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int GetPubKey(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x88;
	pUKey->localTPCCmd.P1 = pKeyAttribute->KeyUse;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x04;
//	pUKey->localTPCCmd.LE = 0x08;	
	pUKey->localTPCCmd.LE = (pKeyAttribute->KeyLen / 8) + 0x08;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
//	memcpy(wr_temp + 2, pFileData->pFileName, pFileData->FileNameLen);
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);

	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x480;
	pUKey->localTDataPack.DATA = pKeyAttribute->pKey;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		int keylen = (rd_temp[3] << 8) + rd_temp[2];
		//memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, (keylen + 6));
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}


/***************************************************************************
 * name: DelKey
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int DelKey(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute){	
		
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xde;
	pUKey->localTPCCmd.P1 = pKeyAttribute->KeyUse;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x04;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
//	memcpy(wr_temp + 2, pFileData->pFileName, pFileData->FileNameLen);
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	
	pUKey->localTDataPack.DATA = wr_temp;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = 0x80;
//	pUKey->localTDataPack.DATA = pFileData->pDataAddr;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
	}


/***************************************************************************
 * name: PubKeyEncrypt
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int PubKeyEncrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x5c;
	pUKey->localTPCCmd.P1 = pKeyAttribute->KeyUse;
	pUKey->localTPCCmd.P2 = 0x01;
	pUKey->localTPCCmd.LC = pOptData->PlainDataLen + 6;
	pUKey->localTPCCmd.LE = pOptData->CipherDataLen;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
	wr_temp[4] = 0x01;
	wr_temp[5] = 0x00;
	memcpy(wr_temp + 6, pOptData->pPlainData, pOptData->PlainDataLen);
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	
	pUKey->localTDataPack.DATA = wr_temp;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = pOptData->CipherDataLen;
	pUKey->localTDataPack.DATA = pOptData->pCipherData;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;	
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}


/***************************************************************************
 * name: PriKeyDecrypt
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int PriKeyDecrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData){	
		
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x5c;
	pUKey->localTPCCmd.P1 = pKeyAttribute->KeyUse;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pOptData->CipherDataLen + 6;
	pUKey->localTPCCmd.LE = pOptData->PlainDataLen;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;
	wr_temp[4] = 0x00;
	wr_temp[5] = 0x00;
	memcpy(wr_temp + 6, pOptData->pCipherData, pOptData->CipherDataLen);
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	
	pUKey->localTDataPack.DATA = wr_temp;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = pOptData->PlainDataLen;;
	pUKey->localTDataPack.DATA = pOptData->pPlainData;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}
/***************************************************************************
 * name: PubKeyDecrypt
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int PubKeyDecrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xEA;
	pUKey->localTPCCmd.P1 = 0;
	pUKey->localTPCCmd.P2 = 0;
	pUKey->localTPCCmd.LC = 268;
	pUKey->localTPCCmd.LE = pOptData->PlainDataLen;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = 0x00;
	wr_temp[1] = 0x04;
	wr_temp[2] = 0x00;
	wr_temp[3] = 0x00;
//print_buf("pKeyAttribute->pKey", pKeyAttribute->pKey, 134);		
	memcpy(wr_temp + 4, pKeyAttribute->pKey + 6, 128);
	memcpy(wr_temp + 132, pKeyAttribute->pKey + 2, 4);
	memcpy(wr_temp + 136, pOptData->pCipherData, 132);
//print_buf("wr_temp", wr_temp, 268);	
	pUKey->localTDataPack.DATA = wr_temp;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
		
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = 0x100;
	pUKey->localTDataPack.DATA = pOptData->pPlainData;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	int i;
//	printf("PubKeyDecrypt-----\n");
//	for(i=0; i<(pUKey->localTPCCmd.LC + 2); i++)
//		printf("%x ", rd_temp[i]);
	printf("\n");

	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, 132);
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}

/***************************************************************************
 * name: PriKeyEncrypt
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int PriKeyEncrypt(struct UKey *pUKey, struct AppAttribute *pAppAttribute,
			struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData){	

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x58;
	pUKey->localTPCCmd.P1 = 0;
	pUKey->localTPCCmd.P2 = 0;
	pUKey->localTPCCmd.LC = pOptData->PlainDataLen + 4;
	pUKey->localTPCCmd.LE = 260;	

	unsigned char wr_temp[pUKey->localTPCCmd.LC];
	wr_temp[0] = pAppAttribute->AppID & 0xff;
	wr_temp[1] = pAppAttribute->AppID >> 8;
	wr_temp[2] = pContAttribute->ContID & 0xff;
	wr_temp[3] = pContAttribute->ContID >> 8;

	memcpy(wr_temp + 4, pOptData->pPlainData, pOptData->PlainDataLen);
	
	pUKey->localTDataPack.DATA = wr_temp;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = pOptData->CipherDataLen;
	pUKey->localTDataPack.DATA = pOptData->pCipherData;
	unsigned char rd_temp[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
//	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxfer_len = 134;
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;	

//	print_buf("pUKey->p_hdr->dxferp", pUKey->p_hdr->dxferp, pUKey->p_hdr->dxfer_len + 4);
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp[1] << 8) + rd_temp[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, 132);
	}else{
		LOG("Receive data error\n");
		return -1;
	}

	return ret;
}
/***************************************************************************
 * name: ImportKey
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int ImportKey(struct UKey *pUKey, unsigned int AlgMode, unsigned char *pInitVec,
			struct AppAttribute *pAppAttribute,	struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute){	
//1\A1\A2\B5\BC\C8\EB\B6Գ\C6\C3\DCԿ	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xa0;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pKeyAttribute->KeyLen + 12;
	pUKey->localTPCCmd.LE = 0x02;	
	
	unsigned char wr_temp1[pUKey->localTPCCmd.LC];
	wr_temp1[0] = pAppAttribute->AppID & 0xff;
	wr_temp1[1] = pAppAttribute->AppID >> 8;
	wr_temp1[2] = pContAttribute->ContID & 0xff;
	wr_temp1[3] = pContAttribute->ContID >> 8;
	wr_temp1[4] = AlgMode & 0xff;
	wr_temp1[5] = (AlgMode >> 8) & 0xff;
	wr_temp1[6] = (AlgMode >> 16) & 0xff;
	wr_temp1[7] = (AlgMode >> 24) & 0xff;
	wr_temp1[8] = pKeyAttribute->KeyLen & 0xff;
	wr_temp1[9] = (pKeyAttribute->KeyLen >> 8) & 0xff;
	wr_temp1[10] = (pKeyAttribute->KeyLen >> 16) & 0xff;
	wr_temp1[11] = (pKeyAttribute->KeyLen >> 24) & 0xff;
	memcpy((wr_temp1 + 12), pKeyAttribute->pKey, pKeyAttribute->KeyLen);
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	int i;
	printf("wr_temp1-----\n");
	for(i=0; i<(pUKey->localTPCCmd.LC); i++)
		printf("%x ", wr_temp1[i]);
	printf("\n");
	pUKey->localTDataPack.DATA = wr_temp1;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = (unsigned char *)&(pKeyAttribute->KeyID);
	unsigned char rd_temp1[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;

	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp1[1] << 8) + rd_temp1[0];
//		memcpy(pUKey->localTDataPack.DATA, rd_temp1+2, pUKey->localTPCCmd.LE);
		pKeyAttribute->KeyID = (rd_temp1[3] << 8) + rd_temp1[2];
	}else{
		LOG("Import KeyPair Receive data error\n");
		return -1;
	}
//	int i;
	printf("rd_temp1-----\n");
	for(i=0; i<(pUKey->localTDataPack.LC); i++)
		printf("%x ", rd_temp1[i]);
	printf("\n");
	return ret;
}
/***************************************************************************
 * name: SymEncrypt
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int SymEncrypt(struct UKey *pUKey, unsigned int AlgMode, unsigned char *pInitVec,
			struct AppAttribute *pAppAttribute,	struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData){	
#if 0
//1\A1\A2\B5\BC\C8\EB\B6Գ\C6\C3\DCԿ	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xa2;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pKeyAttribute->KeyLen + 4;
	pUKey->localTPCCmd.LE = 0x02;	
	
	unsigned char wr_temp1[pUKey->localTPCCmd.LC];
	memcpy(wr_temp1, pKeyAttribute->pKey, pKeyAttribute->KeyLen);
	wr_temp1[pKeyAttribute->KeyLen + 0] = AlgMode & 0xff;
	wr_temp1[pKeyAttribute->KeyLen + 1] = (AlgMode >> 8) & 0xff;
	wr_temp1[pKeyAttribute->KeyLen + 2] = (AlgMode >> 16) & 0xff;
	wr_temp1[pKeyAttribute->KeyLen + 3] = (AlgMode >> 24) & 0xff;
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	
	pUKey->localTDataPack.DATA = wr_temp1;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = (unsigned char *)&(pKeyAttribute->KeyID);
	unsigned char rd_temp1[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;

	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp1[1] << 8) + rd_temp1[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp1+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Import KeyPair Receive data error\n");
		return -1;
	}
	if(ret != 0){
		LOG("Import KeyPair return error\n");
		return -1;
	}
#endif
//2\A1\A2\BC\D3\C3ܳ\F5ʼ\BB\AF	
	struct BlockCipherParam *pBlockCipherParam = (struct BlockCipherParam *)malloc(sizeof(struct BlockCipherParam));
	memset(pBlockCipherParam, 0, sizeof(struct BlockCipherParam));
	pBlockCipherParam->IVLen = 0x10;
	pBlockCipherParam->PaddingType = 0x00;
	pBlockCipherParam->FeedBitLen = 0x00;
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xa4;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = sizeof(struct BlockCipherParam) + 10;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp2[pUKey->localTPCCmd.LC];
	wr_temp2[0] = pAppAttribute->AppID & 0xff;
	wr_temp2[1] = pAppAttribute->AppID >> 8;
	wr_temp2[2] = pContAttribute->ContID & 0xff;
	wr_temp2[3] = pContAttribute->ContID >> 8;
	wr_temp2[4] = pKeyAttribute->KeyID & 0xff;
	wr_temp2[5] = pKeyAttribute->KeyID >> 8;
	wr_temp2[6] = AlgMode & 0xff;
	wr_temp2[7] = (AlgMode >> 8) & 0xff;
	wr_temp2[8] = (AlgMode >> 16) & 0xff;
	wr_temp2[9] = (AlgMode >> 24) & 0xff;
	memcpy(wr_temp2 + 10, pBlockCipherParam, sizeof(struct BlockCipherParam));
	free(pBlockCipherParam);

	pUKey->localTDataPack.DATA = wr_temp2;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;	
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	unsigned char rd_temp2[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp2;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;	
	}
	DELAY;	
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp2[1] << 8) + rd_temp2[0];
	}else{
		LOG("Init Encrypt Receive data error\n");
		return -1;
	}
	if(ret != 0){
		LOG("Init Encrypt return error\n");
		return -1;
	}
	
//3\A1\A2ִ\D0м\D3\C3\DC	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xa8;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pOptData->PlainDataLen + 6;
	pUKey->localTPCCmd.LE = pOptData->CipherDataLen;	

	unsigned char wr_temp3[pUKey->localTPCCmd.LC];
	wr_temp3[0] = pAppAttribute->AppID & 0xff;
	wr_temp3[1] = pAppAttribute->AppID >> 8;
	wr_temp3[2] = pContAttribute->ContID & 0xff;
	wr_temp3[3] = pContAttribute->ContID >> 8;
	wr_temp3[4] = pKeyAttribute->KeyID & 0xff;
	wr_temp3[5] = pKeyAttribute->KeyID >> 8;
	memcpy(wr_temp3 + 6, pOptData->pPlainData, pOptData->PlainDataLen);

	pUKey->localTDataPack.DATA = wr_temp3;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = pOptData->CipherDataLen;
	pUKey->localTDataPack.DATA = pOptData->pCipherData;
	unsigned char rd_temp3[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp3;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp3[1] << 8) + rd_temp3[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp3+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Encrypt Receive data error\n");
		return -1;
	}

	if(ret != 0){
		LOG("Encrypt return error\n");
		return -1;
	}


//4\A1\A2\BD\E1\CA\F8\BC\D3\C3\DC
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xaa;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x06;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp4[pUKey->localTPCCmd.LC];
	wr_temp4[0] = pAppAttribute->AppID & 0xff;
	wr_temp4[1] = pAppAttribute->AppID >> 8;
	wr_temp4[2] = pContAttribute->ContID & 0xff;
	wr_temp4[3] = pContAttribute->ContID >> 8;
	wr_temp4[4] = pKeyAttribute->KeyID & 0xff;
	wr_temp4[5] = pKeyAttribute->KeyID >> 8;
//	memcpy(wr_temp + 6, pOptData->pCipherData, pOptData->PlainDataLen);
	
	pUKey->localTDataPack.DATA = wr_temp4;	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
//	pUKey->localTDataPack.DATA = pOptData->pCipherData;
	unsigned char rd_temp4[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp4;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp4[1] << 8) + rd_temp4[0];
	}else{
		LOG("End Encrypt Receive data error\n");
	}

	return ret;
}


/***************************************************************************
 * name: SymDecrypt
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int SymDecrypt(struct UKey *pUKey, unsigned int AlgMode, unsigned char *pInitVec,
			struct AppAttribute *pAppAttribute,	struct ContAttribute *pContAttribute, 
			struct KeyAttribute *pKeyAttribute, struct OptData *pOptData){	
#if 0
//1\A1\A2\B5\BC\C8\EB\B6Գ\C6\C3\DCԿ	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xa2;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pKeyAttribute->KeyLen + 4;
	pUKey->localTPCCmd.LE = 0x02;	

	unsigned char wr_temp1[pUKey->localTPCCmd.LC];
	memcpy(wr_temp1, pKeyAttribute->pKey, pKeyAttribute->KeyLen);
	wr_temp1[pKeyAttribute->KeyLen + 0] = AlgMode & 0xff;
	wr_temp1[pKeyAttribute->KeyLen + 1] = (AlgMode >> 8) & 0xff;
	wr_temp1[pKeyAttribute->KeyLen + 2] = (AlgMode >> 16) & 0xff;
	wr_temp1[pKeyAttribute->KeyLen + 3] = (AlgMode >> 24) & 0xff;
//	memcpy((wr_temp + pFileData->FileNameLen + 3), &pFileData->DataOffset,4);
//	memcpy((wr_temp + pFileData->FileNameLen + 7), &pFileData->DataLength,4);
	
	pUKey->localTDataPack.DATA = wr_temp1;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	pUKey->localTDataPack.DATA = (unsigned char *)&(pKeyAttribute->KeyID);
	unsigned char rd_temp1[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp1[1] << 8) + rd_temp1[0];
	}else{
		LOG("Import KeyPair Receive data error\n");
		return -1;
	}

	if(ret != 0){
		LOG("Import KeyPair return error\n");
		return -1;
	}
#endif
//2\A1\A2\BD\E2\C3ܳ\F5ʼ\BB\AF	
	struct BlockCipherParam *pBlockCipherParam = (struct BlockCipherParam *)malloc(sizeof(struct BlockCipherParam));
	memset(pBlockCipherParam, 0, sizeof(struct BlockCipherParam));
	pBlockCipherParam->IVLen = 0x10;
	pBlockCipherParam->PaddingType = 0x00;
	pBlockCipherParam->FeedBitLen = 0x00;
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xac;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = sizeof(struct BlockCipherParam) + 10;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp2[pUKey->localTPCCmd.LC];
	wr_temp2[0] = pAppAttribute->AppID & 0xff;
	wr_temp2[1] = pAppAttribute->AppID >> 8;
	wr_temp2[2] = pContAttribute->ContID & 0xff;
	wr_temp2[3] = pContAttribute->ContID >> 8;
	wr_temp2[4] = pKeyAttribute->KeyID & 0xff;
	wr_temp2[5] = pKeyAttribute->KeyID >> 8;
	wr_temp2[6] = AlgMode & 0xff;
	wr_temp2[7] = (AlgMode >> 8) & 0xff;
	wr_temp2[8] = (AlgMode >> 16) & 0xff;
	wr_temp2[9] = (AlgMode >> 24) & 0xff;
	memcpy(wr_temp2 + 10, pBlockCipherParam, sizeof(struct BlockCipherParam));
	free(pBlockCipherParam);
	
	pUKey->localTDataPack.DATA = wr_temp2;	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
//	pUKey->localTDataPack.DATA = &pKeyAttribute->KeyID;
	unsigned char rd_temp2[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp2;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp2[1] << 8) + rd_temp2[0];
	}else{
		LOG("Init Decrypt Receive data error\n");
		return -1;
	}
	if(ret != 0){
		LOG("Init Decrypt return error\n");
		return -1;
	}

//3\A1\A2ִ\D0н\E2\C3\DC
//	pUKey->localTDataPack.LC = 0x0a;
//	pUKey->localTDataPack.DATA = (unsigned char *)pFileData->pDataAddr;
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xb0;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = pOptData->CipherDataLen + 6;
	pUKey->localTPCCmd.LE = pOptData->PlainDataLen;	

	unsigned char wr_temp3[pUKey->localTPCCmd.LC];
	wr_temp3[0] = pAppAttribute->AppID & 0xff;
	wr_temp3[1] = pAppAttribute->AppID >> 8;
	wr_temp3[2] = pContAttribute->ContID & 0xff;
	wr_temp3[3] = pContAttribute->ContID >> 8;
	wr_temp3[4] = pKeyAttribute->KeyID & 0xff;
	wr_temp3[5] = pKeyAttribute->KeyID >> 8;
	memcpy(wr_temp3 + 6, pOptData->pCipherData, pOptData->CipherDataLen);
	
	pUKey->localTDataPack.DATA = wr_temp3;	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = pOptData->PlainDataLen;
	pUKey->localTDataPack.DATA = pOptData->pPlainData;
	unsigned char rd_temp3[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp3;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;	
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp3[1] << 8) + rd_temp3[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp3+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Decrypt Receive data error\n");
		return -1;
	}

	if(ret != 0){
		LOG("Decrypt return error\n");
		return -1;
	}

//4\A1\A2\BD\E1\CA\F8\BD\E2\C3\DC
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xb2;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x06;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp4[pUKey->localTPCCmd.LC];
	wr_temp4[0] = pAppAttribute->AppID & 0xff;
	wr_temp4[1] = pAppAttribute->AppID >> 8;
	wr_temp4[2] = pContAttribute->ContID & 0xff;
	wr_temp4[3] = pContAttribute->ContID >> 8;
	wr_temp4[4] = pKeyAttribute->KeyID & 0xff;
	wr_temp4[5] = pKeyAttribute->KeyID >> 8;
//	memcpy(wr_temp + 6, pOptData->pCipherData, pOptData->PlainDataLen);
	
	pUKey->localTDataPack.DATA = wr_temp4;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;

	pUKey->localTDataPack.LC = 0x400;
//	pUKey->localTDataPack.DATA = pOptData->pCipherData;
	unsigned char rd_temp4[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp4;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;

	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp4[1] << 8) + rd_temp4[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp4+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
		return -1;
	}


	return ret;
}


/***************************************************************************
 * name: InitialUKey
 * parameter:
 * 		pUKey:	
 * 		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
int InitialUKey(struct UKey *pUKey, struct AppAttribute *pAppAttribute,	
			struct ContAttribute *pContAttribute){
//1\A1\A2\BB\F1ȡ\CB\E6\BB\FA\CA\FD
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x50;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x02;
	pUKey->localTPCCmd.LE = 0x08;	
	
	unsigned char wr_temp0[2] = {0x00,0x00};
	pUKey->localTDataPack.DATA = wr_temp0;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	char Random[8];
	pUKey->localTDataPack.LC = 0x08;
	pUKey->localTDataPack.DATA = Random;
	unsigned char rd_temp0[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp0;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp0[1] << 8) + rd_temp0[0];
		memcpy(pUKey->localTDataPack.DATA, rd_temp0+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Get Random Receive data error\n");
		return -1;
	}
	if(ret != 0){
		LOG("Get Random return error\n");
		return -1;
	}
	//ʹ\D3\C3\C9豸\C3\DCԿ\B6\D4\CB\E6\BB\FA\CA\FD\BD\F8\D0\D03DES\BC\D3\C3\DC
	char KeyA[] = {0x23,0x45,0xbb,0x5d,0x67,0x8a,0x9c,0xdd};
//	ecb_crypt(KeyA, (char *)pUKey->localTDataPack.DATA, 8, DES_ENCRYPT);
	des_encrypt(KeyA, (char *)pUKey->localTDataPack.DATA, 8);
//2\A1\A2\C8\CF֤\C9豸\C3\DCԿ	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x10;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x02;
	pUKey->localTPCCmd.LC = 0x08;
	pUKey->localTPCCmd.LE = 0x0a;	

	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	unsigned char rd_temp1[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp1[1] << 8) + rd_temp1[0];
	}else{
		LOG("AuthdevKey Receive data error\n");
		return -1;
	}
	
	if(ret != 0){
		LOG("AuthdevKey return error\n");
		return -1;
	}

//3\A1\A2\B4\F2\BF\AAӦ\D3\C3
	const char app_name[] = {'c','a','_','a','p','p'};
	pUKey->localTDataPack.DATA = (unsigned char *)app_name;
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x26;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x06;
	pUKey->localTPCCmd.LE = 0x0a;	
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;	
	unsigned char rd_temp2[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp2;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp2[1] << 8) + rd_temp2[0];		
	}else{
		LOG("OpenApp Receive data error\n");
	}
	if(ret != 0){
		LOG("OpenApp return error\n");
		return -1;
	}

//4\A1\A2\C8\F4\B4\F2\BF\AAӦ\D3óɹ\A6\D4\F2ɾ\B3\FDӦ\D3\C3
	pUKey->localTDataPack.DATA = (unsigned char *)app_name;
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x24;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x06;
	pUKey->localTPCCmd.LE = 0x0a;	
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;	
	unsigned char rd_temp3[pUKey->localTPCCmd.LE + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp3;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp3[1] << 8) + rd_temp3[0];	
	}else{
		LOG("DelApp Receive data error\n");
	}
	if(ret != 0){
		LOG("DelApp return error\n");
		return -1;
	}

//5\A1\A2\D0½\A8Ӧ\D3\C3
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x20;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x60;
	pUKey->localTPCCmd.LE = 0x0a;	

	unsigned char wr_temp4[96] = {
	 0x63,0x61,0x5F,0x61,0x70,0x70,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x31,0x31
	,0x31,0x31,0x31,0x31,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x0A,0x00,0x00,0x00,0x32,0x32
	,0x32,0x32,0x32,0x32,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x0A,0x00,0x00,0x00,0xFF,0x00
	,0x00,0x00,0x00,0x10,0x10,0x00};
	
	pUKey->localTDataPack.DATA = wr_temp4;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	pUKey->localTDataPack.LC = 0x400;
	unsigned char rd_temp4[pUKey->localTDataPack.LC + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp4;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp4[1] << 8) + rd_temp4[0];	
	}else{
		LOG("NewApp Receive data error\n");
	}

	if(ret != 0){
		LOG("NewApp return error\n");
		return -1;
	}
	return ret;

#if 0

//5\A1\A2\B4\F2\BF\AAӦ\D3\C3

	pUKey->localTDataPack.DATA = app_name;

	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x26;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x06;
	pUKey->localTPCCmd.LE = 0x0a;	

	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
	}

	pUKey->localTPCCmd.LE = 0x400;
	unsigned char rd_temp5[pUKey->localTPCCmd.LE + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTPCCmd.LE ? (pUKey->localTPCCmd.LE + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp5;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
	}

#if 0	
	if (pUKey->p_hdr->dxfer_len){
		LOG("Receive data %d \n",pUKey->p_hdr->dxfer_len);
		memcpy(pUKey->localTDataPack.DATA, rd_temp+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
	}
#endif
//6\A1\A2\B4\B4\BD\A8\CEļ\FE
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x30;
	pUKey->localTPCCmd.P1 = rd_temp5[11];
	pUKey->localTPCCmd.P2 = rd_temp5[10];
	pUKey->localTPCCmd.LC = 0x3c;
	pUKey->localTPCCmd.LE = 0x00;	
	
	unsigned char wr_temp6[60] = {
	 0x43,0x52,0x69,0x67,0x68,0x74,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00
	,0x00,0x00,0x10,0x00,0x00,0x00,0x01,0x00,0x00,0x00};
	
	pUKey->localTDataPack.DATA = wr_temp6;
	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
	}

	pUKey->localTPCCmd.LE = 0x400;
	unsigned char rd_temp6[pUKey->localTPCCmd.LE + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTPCCmd.LE ? (pUKey->localTPCCmd.LE + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp6;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
	}

#if 0	
	if (pUKey->p_hdr->dxfer_len){
		LOG("Receive data %d \n",pUKey->p_hdr->dxfer_len);
		memcpy(pUKey->localTDataPack.DATA, rd_temp4+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
	}
#endif

//7\A1\A2\C8\CF֤\C9豸\C3\DCԿ	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x10;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x08;
	pUKey->localTPCCmd.LE = 0x10;	

	//unsigned char wr_temp7[8] = {0x7e,0xab,0xa1,0xb7,0xe6,0xd6,0x8e,0xf0};
	pUKey->localTDataPack.DATA = wr_temp1;

	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
	}

	pUKey->localTPCCmd.LE = 0x400;
	//unsigned char rd_temp7[pUKey->localTPCCmd.LE + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTPCCmd.LE ? (pUKey->localTPCCmd.LE + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
	}

#if 0	
	if (pUKey->p_hdr->dxfer_len){
		LOG("Receive data %d \n",pUKey->p_hdr->dxfer_len);
		memcpy(pUKey->localTDataPack.DATA, rd_temp1+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
	}
#endif
//8\A1\A2\B4\B4\BD\A8\C8\DD\C6\F7
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0x40;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x09;
	pUKey->localTPCCmd.LE = 0x02;	

	unsigned char wr_temp8[9] = {rd_temp5[10],rd_temp5[11],0x6b,0x65,0x79,0x5f,0x63,0x6f,0x6e};
	pUKey->localTDataPack.DATA = wr_temp8;

	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
	}

	pUKey->localTPCCmd.LE = 0x400;
	//unsigned char rd_temp7[pUKey->localTPCCmd.LE + 2];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTPCCmd.LE ? (pUKey->localTPCCmd.LE + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
	}

#if 0	
	if (pUKey->p_hdr->dxfer_len){
		LOG("Receive data %d \n",pUKey->p_hdr->dxfer_len);
		memcpy(pUKey->localTDataPack.DATA, rd_temp1+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Receive data error\n");
	}
#endif
#endif
	return pUKey->p_hdr->status;
	}


	
#if 1	
/***************************************************************************
 * name: SymDecrypt
 * parameter:
 *		pUKey:	
 *		pContAttribute:
 *		pKeyAttribute:
 * function:
 * **************************************************************************/
unsigned char * Hash(struct UKey *pUKey, unsigned char HashType, 
						struct HashData *pHashData){	
	int i;
//1\A1\A2\B5\BC\C8\EB\B6Գ\C6\C3\DCԿ	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xB4;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = HashType;
	pUKey->localTPCCmd.LC = 0x02;
	pUKey->localTPCCmd.LE = 0x00;	

	unsigned char wr_temp1[256];
	pUKey->localTDataPack.DATA = wr_temp1;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return Null;
	}
	DELAY;
	pUKey->localTDataPack.LC = 256;
//	pUKey->localTDataPack.DATA = (unsigned char *)&(pKeyAttribute->KeyID);
	unsigned char rd_temp1[300];
	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return Null;
	}
	DELAY;
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp1[1] << 8) + rd_temp1[0];
	}else{
		LOG("Import KeyPair Receive data error\n");
		return Null;
	}

	if(ret != 0){
		LOG("Import KeyPair return error\n");
		return Null;
	}

//2\A1\A2\BD\E2\C3ܳ\F5ʼ\BB\AF 
    int BlkNum = pHashData->len / 256;
    int RemLen = pHashData->len % 256;
    int Offset = 0;

	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xb8;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 256;
	pUKey->localTPCCmd.LE = 0x00;	
	
	for (i = 0; i < BlkNum; i++){
		memcpy(wr_temp1, (pHashData->data + Offset), 256);
		pUKey->localTDataPack.DATA = wr_temp1;	
		pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
		pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
		pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
		pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
		pUKey->p_hdr->cmd_len = 10;
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
		pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
		
		int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);

		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
		pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
		if (ret<0) {
			LOG("Sending SCSI Write Command failed.\n");
			return Null;
		}
		Offset += 256;

		DELAY;
		pUKey->localTRespond.CMD = 0xef02;//read	
		pUKey->localTRespond.SW = 0x00;
		pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
		pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
		pUKey->p_hdr->dxferp = rd_temp1;
		pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
		pUKey->p_hdr->cmd_len = 10;
		
		ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
		if (ret<0) {
			LOG("Sending SCSI Read Command failed.\n");
			return Null;
		}
		DELAY;
		if (pUKey->p_hdr->dxfer_len){
			ret = (rd_temp1[1] << 8) + rd_temp1[0];
		}else{
			LOG("Init Decrypt Receive data error\n");
			return Null;
		}
		if(ret != 0){
			LOG("Init Decrypt return error\n");
			return Null;
		}
	}
	if (RemLen > 0){

		pUKey->localTPCCmd.LC = RemLen;
		memcpy(wr_temp1, (pHashData->data + Offset), RemLen);
		pUKey->localTDataPack.DATA = wr_temp1;	
		pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
		pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
		pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
		pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
		pUKey->p_hdr->cmd_len = 10;
		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
		pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
		
		int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);

		pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
		pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
		if (ret<0) {
			LOG("Sending SCSI Write Command failed.\n");
			return Null;
		}

		DELAY;
		pUKey->localTRespond.CMD = 0xef02;//read	
		pUKey->localTRespond.SW = 0x00;
		pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
		pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
		pUKey->p_hdr->dxferp = rd_temp1;
		pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
		pUKey->p_hdr->cmd_len = 10;
		
		ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
		if (ret<0) {
			LOG("Sending SCSI Read Command failed.\n");
			return Null;
		}
		DELAY;
		if (pUKey->p_hdr->dxfer_len){
			ret = (rd_temp1[1] << 8) + rd_temp1[0];
		}else{
			LOG("Init Decrypt Receive data error\n");
			return Null;
		}
		if(ret != 0){
			LOG("Init Decrypt return error\n");
			return Null;
		}
	}
		


//3\A1\A2ִ\D0н\E2\C3\DC
//	pUKey->localTDataPack.LC = 0x0a;
//	pUKey->localTDataPack.DATA = (unsigned char *)pFileData->pDataAddr;
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x80;
	pUKey->localTPCCmd.INS = 0xba;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 2;
	pUKey->localTPCCmd.LE = 0; 

	wr_temp1[0] = 0;
	wr_temp1[1] = 0;

	pUKey->localTDataPack.DATA = wr_temp1;	
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = pUKey->localTPCCmd.LC;
	pUKey->p_hdr->dxferp = pUKey->localTDataPack.DATA;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10;
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
			pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		LOG("Sending SCSI Write Command failed.\n");
		return Null;
	}
	DELAY;

	pUKey->localTRespond.CMD = 0xef02;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	pUKey->p_hdr->dxfer_len = (pUKey->localTDataPack.LC ? (pUKey->localTDataPack.LC + 2) : 0);
	pUKey->p_hdr->dxferp = rd_temp1;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;
	
	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		LOG("Sending SCSI Read Command failed.\n");
		return Null;
	}
	DELAY;	
	if (pUKey->p_hdr->dxfer_len){
		ret = (rd_temp1[1] << 8) + rd_temp1[0];
//		memcpy(pUKey->localTDataPack.DATA, rd_temp3+2, pUKey->localTPCCmd.LE);
	}else{
		LOG("Decrypt Receive data error\n");
		return Null;
	}

	if(ret != 0){
		LOG("Decrypt return error\n");
		return Null;
	}else{
		return (rd_temp1 + 6);
	}
}

#endif
