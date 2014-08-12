#include <scsi/scsi_ioctl.h>
#include "common.h"
#include "as532.h"
#include <scsi/sg.h>


/***************************************************************************
 * name: init_io_hdr
 * parameter:
 * function: initialize the sg_io_hdr struct fields with the most common
 * 			 value
 * **************************************************************************/
void init_io_hdr(struct sg_io_hdr * p_scsi_hdr) {

	memset(p_scsi_hdr, 0, sizeof(struct sg_io_hdr));
	if (p_scsi_hdr) {
		p_scsi_hdr->interface_id = 'S'; /* this is the only choice we have! */
		p_scsi_hdr->flags = SG_FLAG_LUN_INHIBIT; /* this would put the LUN to 2nd byte of cdb*/
	}
}
 
struct  sg_io_hdr * alloc_io_hdr() {

	struct sg_io_hdr * p_scsi_hdr = (struct sg_io_hdr *)malloc(sizeof(struct sg_io_hdr));
	memset(p_scsi_hdr, 0, sizeof(struct sg_io_hdr));
	if (p_scsi_hdr) {
		p_scsi_hdr->interface_id = 'S'; /* this is the only choice we have! */
		p_scsi_hdr->flags = SG_FLAG_LUN_INHIBIT; /* this would put the LUN to 2nd byte of cdb*/
	}

	return p_scsi_hdr;
}

void init_key(struct  UKey * pUKey)
{
	init_UKey(pUKey);
	init_fd(pUKey,&global_sg_fd);
	init_io_hdr(pUKey->p_hdr);

}

void destroy_io_hdr(struct sg_io_hdr * p_hdr) {
	if (p_hdr) {
		free(p_hdr);
		//p_hdr = NULL;
	}
}

void init_fd(struct  UKey * pUKey,int *fd)
{
	pUKey->fd = *fd;
}

void init_UKey(struct  UKey * pUKey) {
	//不能清除最后一个成员
	//memset(pUKey, 0, sizeof(struct UKey));
	memset(pUKey, 0, sizeof(int)+sizeof(struct TPCCmd)+sizeof(struct TRespond)+sizeof(struct TDataPack));
}

struct  UKey *alloc_UKey() {
	struct UKey *pUKey = (struct UKey *)malloc(sizeof(struct UKey));
	memset(pUKey, 0, sizeof(struct UKey));
	return pUKey;
}

void destroy_UKey(struct UKey * pUKey) {
	if (pUKey) {
		free(pUKey);
		//pUKey = NULL;
	}
}

void destroy_key(struct UKey * pUKey)
{	
	destroy_io_hdr(pUKey->p_hdr);
	destroy_UKey(pUKey);
}

/*
name: int key(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
fun: 读取版本号  
para:  struct UKey *pUKey  :IN USB控制相关结构体
       unsigned char *pUsbData: OUT读取USB的数据指针
       int *pUsbDataLen: OUT 读取的USB数据的长度
ret: 0，成功； 其他值:失败
other: 版本号的规则0xA50x5A.....0xA50x5A
*/
int key(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
{
}

/*
int lcd(struct UKey *pUKey,  unsigned char *pUsbData) 
fun: 向LCD中写数据 
para:  struct UKey *pUKey  :IN USB控制相关结构体
       unsigned char *pUsbData:IN 向LCD屏写数据的的数据指针 
ret: 0，成功； 其他值:失败
other:  
*/
int lcd(struct UKey *pUKey,  unsigned char *pUsbData) 
{   
	return 0;
}

/*
name: int version(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
fun: 读取版本号  
para:  struct UKey *pUKey  :IN USB控制相关结构体
       unsigned char *pUsbData: OUT读取USB的数据指针
       int *pUsbDataLen: OUT 读取的USB数据的长度
ret: 0，成功； 其他值:失败
other: 版本号的规则0xA50x5A.....0xA50x5A
*/
int version(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
{
    PRINT("-------------------------------version----------------cCmd = 0x02----------------\n");   
    unsigned int i;
    unsigned short int len;
    unsigned char wrBuf[5]={0x00, 0xff, 0x00, 0x00, 0xff};
    unsigned char rd_temp[1000] ={0} ;
	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x01;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x02;
	pUKey->localTPCCmd.Resved = 0x01;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x04;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00;
    
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = sizeof(wrBuf);//(unsigned int)((pUKey->localTPCCmd.cLen[0]<<8) + (pUKey->localTPCCmd.cLen[1]));
	pUKey->p_hdr->dxferp = wrBuf;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    PRINT("-------------wr-----------\n");
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}
    PRINT("Sending SCSI Write Command success.\n");
	DELAY;
   
  	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x03;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x01;
	pUKey->localTPCCmd.Resved = 0x00;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x00;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00; 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = sizeof(rd_temp); //要读取数据的长度，最长能读取1000个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
    PRINT("Sending SCSI Read Command success.\n");
	DELAY;
	if(rd_temp[0] != 0xa5 || rd_temp[1] != 0x5a)
		return -1;
    len=(unsigned short int)(( rd_temp[5]<<8)+rd_temp[6]);
    len=len+8;
    PRINT("rd_temp[5] = %x\n", rd_temp[5]);
    PRINT("rd_temp[6] = %x\n", rd_temp[6]);
    PRINT("len = %d\n", len);
 
    if (pUKey->p_hdr->dxfer_len)
    {
		PRINT("-------------\n");
        for(i =0; i<len; i++)
        printf(" %02x", rd_temp[i] );
        PRINT("-------------\n");  	
    }
    memcpy(pUsbData,&rd_temp[7],len-2);
    *pUsbDataLen = len-10;
    return ret;
}

int sn(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
{   
    PRINT("-------------------------------version--detail-----------cCmd = 0x03;-------------------\n");   
    unsigned int i;
    unsigned short int len;   
    unsigned char wrBuf[5]={0x00, 0xff, 0x00, 0x00, 0xff};
    unsigned char rd_temp[1000] ={0} ;
	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x01;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x03;
	pUKey->localTPCCmd.Resved = 0x02;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x04;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00;
    
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = sizeof(wrBuf);//(unsigned int)((pUKey->localTPCCmd.cLen[0]<<8) + (pUKey->localTPCCmd.cLen[1]));
	pUKey->p_hdr->dxferp = wrBuf;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    PRINT("-------------wr-----------\n");
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}
    PRINT("Sending SCSI Write Command success.\n");
	DELAY;
   
  	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x03;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x01;
	pUKey->localTPCCmd.Resved = 0x00;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x00;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00; 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = sizeof(rd_temp); //要读取数据的长度，最长能读取1000个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
    PRINT("Sending SCSI Read Command success.\n");
	DELAY;
	if(rd_temp[0] != 0xa5 || rd_temp[1] != 0x5a)
		return -1;
    len=(unsigned short int)((rd_temp[5]<<8)+rd_temp[6]);
    len=len+8;
    PRINT("rd_temp[5] = %x\n", rd_temp[5]);
    PRINT("rd_temp[6] = %x\n", rd_temp[6]);
    PRINT("len = %d\n", len);
    if (pUKey->p_hdr->dxfer_len)
    {
		PRINT("-------------\n");
        for(i =0; i<len; i++)
        printf(" rd_temp[%d] = %x\n ",i, rd_temp[i] );
        PRINT("-------------\n");  
 	
    }   
	*pUsbDataLen = len-10;
    memcpy(pUsbData,&rd_temp[7],len-2);
	
    return ret;
    
}


int version_detail(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
{
    PRINT("-------------------------------version--detail-----------cCmd = 0x05;-------------------\n");   
    unsigned int i;
    unsigned short int len;   
    unsigned char wrBuf[5]={0x00, 0xff, 0x00, 0x00, 0xff};
    unsigned char rd_temp[1000] ={0} ;
	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x01;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x05;
	pUKey->localTPCCmd.Resved = 0x02;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x04;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00;
    
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = sizeof(wrBuf);//(unsigned int)((pUKey->localTPCCmd.cLen[0]<<8) + (pUKey->localTPCCmd.cLen[1]));
	pUKey->p_hdr->dxferp = wrBuf;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    PRINT("-------------wr-----------\n");
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}
    PRINT("Sending SCSI Write Command success.\n");
	DELAY;
   
  	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x03;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x01;
	pUKey->localTPCCmd.Resved = 0x00;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x00;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00; 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = sizeof(rd_temp); //要读取数据的长度，最长能读取1000个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
    PRINT("Sending SCSI Read Command success.\n");
	DELAY;
	if(rd_temp[0] != 0xa5 || rd_temp[1] != 0x5a)
		return -1;
    len=(unsigned short int)((rd_temp[5]<<8)+rd_temp[6]);
    len=len+8;
    PRINT("rd_temp[5] = %x\n", rd_temp[5]);
    PRINT("rd_temp[6] = %x\n", rd_temp[6]);
    PRINT("len = %d\n", len);
    if (pUKey->p_hdr->dxfer_len)
    {
		PRINT("-------------\n");
        for(i =0; i<len; i++)
        printf(" rd_temp[%d] = %x\n ",i, rd_temp[i] );
        PRINT("-------------\n");  
 	
    }   
	*pUsbDataLen = len-10;
    memcpy(pUsbData,&rd_temp[7],len-2);
	
    return ret;
    
}


int reboot(struct UKey *pUKey)
{   
	return 0;
}

int reboot_enter_boot(struct UKey *pUKey)
{   
    PRINT("-------------------------------InBoot---------cCmd = 0x04;---------------------\n");   
    unsigned int i;
    unsigned short int len;   
    unsigned char wrBuf[5]={0x00, 0xff, 0x00, 0x00, 0xff};
    unsigned char rd_temp[1000] ={0} ;
	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x01;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x04;
	pUKey->localTPCCmd.Resved = 0x04;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x04;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00;
    
	pUKey->p_hdr->dxfer_direction = SG_DXFER_TO_DEV;
	pUKey->p_hdr->dxfer_len = sizeof(wrBuf);//(unsigned int)((pUKey->localTPCCmd.cLen[0]<<8) + (pUKey->localTPCCmd.cLen[1]));
	pUKey->p_hdr->dxferp = wrBuf;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    PRINT("-------------wr-----------\n");
	int ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		//return -1;
	}
	else
		PRINT("Sending SCSI Write Command success.\n");
	DELAY;
   
  	pUKey->localTPCCmd.cflag[0] = 0xA5 ;//write	
	pUKey->localTPCCmd.cflag[1] = 0x5A;//write  a5 5a
	pUKey->localTPCCmd.cHead[0] = 0xef ;//write	
	pUKey->localTPCCmd.cHead[1] = 0x03;//write  ef 01 
	pUKey->localTPCCmd.cCmd = 0x01;
	pUKey->localTPCCmd.Resved = 0x00;//01 00 
	pUKey->localTPCCmd.cParam = 0x00;
	pUKey->localTPCCmd.cLen[0] = 0x00; // 00 00 	                                        //00 07 00 00 00 00 00 00
	pUKey->localTPCCmd.cLen[1] = 0x00;
    
    pUKey->localTPCCmd.buf[0]=0x00;
    pUKey->localTPCCmd.buf[1]=0x00;
    pUKey->localTPCCmd.buf[2]=0x00;
    pUKey->localTPCCmd.buf[3]=0x00;
    pUKey->localTPCCmd.buf[4]=0x00;
    pUKey->localTPCCmd.buf[5]=0x00; 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = sizeof(rd_temp); //要读取数据的长度，最长能读取1000个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 0x10;
    ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
    PRINT("Sending SCSI Read Command success.\n");
	DELAY;
	if(rd_temp[0] != 0xa5 || rd_temp[1] != 0x5a)
		return -1;
     len=(unsigned short int)((rd_temp[5]<<8)+rd_temp[6]);
     len=len+8;
         PRINT("rd_temp[5] = %x\n", rd_temp[5]);
    PRINT("rd_temp[6] = %x\n", rd_temp[6]);
     PRINT("len = %d\n", len);
    if (pUKey->p_hdr->dxfer_len)
    {
		PRINT("-------------\n");
        for(i =0; i<len; i++)
         printf(" %02x", rd_temp[i] );
        PRINT("-------------\n");  
 	
    }   
    return ret;
    
}

/*
name:int test_spi(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
fun: 测试spi 
para:  struct UKey *pUKey  :IN USB控制相关结构体
       unsigned char *pUsbData: OUT读取USB的数据指针， 输出数据格式1111(去除开始头和结尾头)
       int *pUsbDataLen: OUT 读取的USB数据的长度
ret: 0，成功； 其他值:失败
other: 0xA50x5Aokok0xA50x5A 0xA50x5Afail0xA50x5A
*/

int testSpi(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
{
	return 0;   
}


  
