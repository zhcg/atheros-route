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
     unsigned char readbuf[16]= {0};
    
    int ret =0;    
    int len =0;
    int i =0;
   // unsigned char buff[100] = {0};
    int head_addr =0;
    int head_flag = 0;
    int tail_addr =0;
    int tail_flag =0;
  
    PRINT("-------------- pUKey->localTPCCmd.CMD = 0xef01--------------\n");
    //发送命令1，
    pUKey->localTPCCmd.CMD = 0xef01;//write
  	pUKey->localTPCCmd.CLA = 0x11;
  	pUKey->localTPCCmd.INS = 0x08;
  	pUKey->localTPCCmd.P1 = 0x00;
  	pUKey->localTPCCmd.P2 = 0x00;
  	pUKey->localTPCCmd.LC = 0x00;
  	pUKey->localTPCCmd.LE = 0x08;	 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_NONE;

	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10; //指向 SCSI 命令的 cmdp 的字节长度。
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
    
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}	 

  //读取USB 上的的数据，
   pUKey->localTDataPack.DATA = readbuf;
	unsigned char rd_temp[100] ={0} ;
	pUKey->localTRespond.CMD = 0xef03;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = 100; //要读取数据的长度，最长能读取100个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
    
	if (pUKey->p_hdr->dxfer_len)
    {
		PRINT("-------------\n");
        for(i =0; i<100; i++)
        printf(" rd_temp[%d] = %x\n ",i, rd_temp[i] );
        PRINT("-------------\n");  

        //对于读取的数据，按照数据包的形式开始进行去除冗余数据，数据的开头和结尾都必须以0xA5 0x5A
        for(i =0; i<100; )
        {
            if((rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 0 ))
            {
                head_addr = i;
                head_flag = 1;
                PRINT("find head !\n head = %d\n", head_addr);                
            }  
            i++;
            if( (rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 1 )&&(tail_flag == 0 ))
            {
                tail_addr = i+1;               
                PRINT("find tail!\ntail = %d\n", tail_addr );
                tail_flag =1;
            }  
            if((head_flag == 1)&&(tail_flag == 1))
            {
                PRINT("fin head and tail\n");
                break;
            }            
        }

        len = tail_addr - head_addr+1;        
        PRINT("len = %d\n", len);      
       *pUsbDataLen = len;
        PRINT("pUsbDataLen = %d\n", *pUsbDataLen);
        memcpy(pUsbData, rd_temp, len);
        
        for(i =0; i<len; i++)
        {
            printf("pUsbData = %x\n ",*(pUsbData+i)); 
        }
        PRINT("-------------\n");        
	}
    else
    {
		PRINT("Receive data error\n");
		return -1;
	}
	return ret;
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
	
	pUKey->localTPCCmd.CMD = 0xef01;//write
	pUKey->localTPCCmd.CLA = 0x12;
	pUKey->localTPCCmd.INS = 0x08;
	pUKey->localTPCCmd.P1 = 0x00;
	pUKey->localTPCCmd.P2 = 0x00;
	pUKey->localTPCCmd.LC = 0x0B;
	pUKey->localTPCCmd.LE = 0x08;	
    
	pUKey->localTDataPack.DATA = pUsbData;
	
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
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}
	DELAY;
	return ret;
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
    unsigned char readbuf[16]= {0};
    
    int ret =0;    
    int len =0;
    int i =0;
  //  unsigned char buff[100] = {0};
    int head_addr =0;
    int head_flag = 0;
    int tail_addr =0;
    int tail_flag =0;
  
    PRINT("-------------- pUKey->localTPCCmd.CMD = 0xef01--------------\n");
    //发送命令1，
    pUKey->localTPCCmd.CMD = 0xef01;//write
  	pUKey->localTPCCmd.CLA = 0x13;
  	pUKey->localTPCCmd.INS = 0x08;
  	pUKey->localTPCCmd.P1 = 0x00;
  	pUKey->localTPCCmd.P2 = 0x00;
  	pUKey->localTPCCmd.LC = 0x00;
  	pUKey->localTPCCmd.LE = 0x08;	 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_NONE;

	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10; //指向 SCSI 命令的 cmdp 的字节长度。
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
    
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}	 

  //读取USB 上的的数据，
    pUKey->localTDataPack.DATA = readbuf;
	unsigned char rd_temp[100] ={0} ;
	pUKey->localTRespond.CMD = 0xef03;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = 100; //要读取数据的长度，最长能读取100个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
    
	if (pUKey->p_hdr->dxfer_len)
    {
        //对于读取的数据，按照数据包的形式开始进行去除冗余数据，数据的开头和结尾都必须以0xA5 0x5A
        for(i =0; i<100; )
        {
            if((rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 0 ))
            {
                head_addr = i+2;
                head_flag = 1;
                PRINT("find head !\n head = %d\n", head_addr);                
            }  
            i++;
            if( (rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 1 )&&(tail_flag == 0 ))
            {
                tail_addr = i-1;               
                PRINT("find tail!\ntail = %d\n", tail_addr );
                tail_flag =1;
            }  
            if((head_flag == 1)&&(tail_flag == 1))
            {
                PRINT("find head and tail\n");
                break;
            }            
        }

        len = tail_addr - head_addr+1;        
        //PRINT("len = %d\n", len);      
       *pUsbDataLen = len;
        //PRINT("pUsbDataLen = %d\n", *pUsbDataLen);
        memcpy(pUsbData, rd_temp+head_addr, len);
        
        //for(i =0; i<len; i++)
        //{
            //printf("pUsbData = %x\n ",*(pUsbData+i)); 
        //}
        //PRINT("-------------\n");        
	}
    else
    {
		PRINT("Receive data error\n");
		return -1;
	}
	return ret;
}

int version_detail(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
{   
    unsigned char readbuf[1000]= {0};
    
    int ret =0;    
    int len =0;
    int i =0;
 
    int head_addr =0;
    int head_flag = 0;
    int tail_addr =0;
    int tail_flag =0;
  
    PRINT("-------------- pUKey->localTPCCmd.CMD = 0xef01--------------\n");
    //发送命令1，
    pUKey->localTPCCmd.CMD = 0xef01;//write
  	pUKey->localTPCCmd.CLA = 0x14;
  	pUKey->localTPCCmd.INS = 0x08;
  	pUKey->localTPCCmd.P1 = 0x00;
  	pUKey->localTPCCmd.P2 = 0x00;
  	pUKey->localTPCCmd.LC = 0x00;
  	pUKey->localTPCCmd.LE = 0x08;	 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_NONE;

	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10; //指向 SCSI 命令的 cmdp 的字节长度。
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
    
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}	 

  //读取USB 上的的数据，
    pUKey->localTDataPack.DATA = readbuf;
	unsigned char rd_temp[1000] ={0} ;
	pUKey->localTRespond.CMD = 0xef03;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = 1000; //要读取数据的长度，最长能读取1000个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
    
	if (pUKey->p_hdr->dxfer_len)
    {/*
		PRINT("-------------\n");
        for(i =0; i<1000; i++)
        printf(" rd_temp[%d] = %x\n ",i, rd_temp[i] );
        PRINT("-------------\n");  
*/
        //对于读取的数据，按照数据包的形式开始进行去除冗余数据，数据的开头和结尾都必须以0xA5 0x5A
        for(i =0; i<1000; )
        {
            if((rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 0 ))
            {
                head_addr = i+2;
                head_flag = 1;
                PRINT("find head !\n head = %d\n", head_addr);                
            }  
            i++;
            if( (rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 1 )&&(tail_flag == 0 ))
            {
                tail_addr = i-1;               
                PRINT("find tail!\ntail = %d\n", tail_addr );
                tail_flag =1;
            }  
            if((head_flag == 1)&&(tail_flag == 1))
            {
                PRINT("find head and tail\n");
                break;
            }            
        }

        len = tail_addr - head_addr+1;        
        //PRINT("len = %d\n", len);      
       *pUsbDataLen = len;
        //PRINT("pUsbDataLen = %d\n", *pUsbDataLen);
        memcpy(pUsbData, rd_temp+head_addr, len);
        
        //for(i =0; i<len; i++)
        //{
            //printf("pUsbData = %x\n ",*(pUsbData+i)); 
        //}
        //PRINT("-------------\n");        
	}
    else
    {
		PRINT("Receive data error\n");
		return -1;
	}
	return ret;
}


int reboot(struct UKey *pUKey)
{   
    int ret =0;    
    PRINT("-------------- pUKey->localTPCCmd.CMD = 0xef01--------------\n");
    //发送命令1，
    pUKey->localTPCCmd.CMD = 0xef01;//write
  	pUKey->localTPCCmd.CLA = 0x16;
  	pUKey->localTPCCmd.INS = 0x08;
  	pUKey->localTPCCmd.P1 = 0x00;
  	pUKey->localTPCCmd.P2 = 0x00;
  	pUKey->localTPCCmd.LC = 0x00;
  	pUKey->localTPCCmd.LE = 0x08;	 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_NONE;

	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10; //指向 SCSI 命令的 cmdp 的字节长度。
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
    
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}	   
}

int reboot_enter_boot(struct UKey *pUKey)
{   
    int ret =0;      
    PRINT("-------------- pUKey->localTPCCmd.CMD = 0xef01--------------\n");
    //发送命令1，
    pUKey->localTPCCmd.CMD = 0xef01;//write
  	pUKey->localTPCCmd.CLA = 0x15;
  	pUKey->localTPCCmd.INS = 0x08;
  	pUKey->localTPCCmd.P1 = 0x00;
  	pUKey->localTPCCmd.P2 = 0x00;
  	pUKey->localTPCCmd.LC = 0x00;
  	pUKey->localTPCCmd.LE = 0x08;	 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_NONE;

	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10; //指向 SCSI 命令的 cmdp 的字节长度。
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
    
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}	   
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
    unsigned char readbuf[1000]= {0};
    
    int ret =0;    
    int len =0;
    int i =0;
 
    int head_addr =0;
    int head_flag = 0;
    int tail_addr =0;
    int tail_flag =0;
  
    PRINT("-------------- pUKey->localTPCCmd.CMD = 0xef01--------------\n");
    //发送命令1，
    pUKey->localTPCCmd.CMD = 0xef01;//write
  	pUKey->localTPCCmd.CLA = 0x16;
  	pUKey->localTPCCmd.INS = 0x08;
  	pUKey->localTPCCmd.P1 = 0x00;
  	pUKey->localTPCCmd.P2 = 0x00;
  	pUKey->localTPCCmd.LC = 0x00;
  	pUKey->localTPCCmd.LE = 0x08;	 

	pUKey->p_hdr->dxfer_direction = SG_DXFER_NONE;

	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTPCCmd);
	pUKey->p_hdr->cmd_len = 10; //指向 SCSI 命令的 cmdp 的字节长度。
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	pUKey->localTPCCmd.LC = L2B16(pUKey->localTPCCmd.LC);
	pUKey->localTPCCmd.LE = L2B16(pUKey->localTPCCmd.LE);
	if (ret<0) {
		PRINT("Sending SCSI Write Command failed.\n");
		return -1;
	}	 

  //读取USB 上的的数据，
    pUKey->localTDataPack.DATA = readbuf;
	unsigned char rd_temp[1000] ={0} ;
	pUKey->localTRespond.CMD = 0xef03;//read	
	pUKey->localTRespond.SW = 0x00;
	pUKey->p_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    pUKey->p_hdr->dxfer_len = 1000; //要读取数据的长度，最长能读取1000个字节的数据
	pUKey->p_hdr->dxferp = rd_temp;
	pUKey->p_hdr->cmdp = (unsigned char *)&(pUKey->localTRespond);
	pUKey->p_hdr->cmd_len = 10;

	ret = ioctl(pUKey->fd, SG_IO, pUKey->p_hdr);
	if (ret<0) {
		PRINT("Sending SCSI Read Command failed.\n");
		return -1;
	}
	DELAY;
    
	if (pUKey->p_hdr->dxfer_len)
    {
		//PRINT("-------------\n");
        //for(i =0; i<150; i++)
        //printf(" rd_temp[%d] = %x\n ",i, rd_temp[i] );
        //PRINT("-------------\n");  

        //对于读取的数据，按照数据包的形式开始进行去除冗余数据，数据的开头和结尾都必须以0xA5 0x5A
        for(i =0; i<1000; )
        {
            if((rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 0 ))
            {
                head_addr = i+2;
                head_flag = 1;
                PRINT("find head !\n head = %d\n", head_addr);                
            }  
            i++;
            if( (rd_temp[i]== 0xA5)&&(rd_temp[i+1]==0x5A)&&(head_flag == 1 )&&(tail_flag == 0 ))
            {
                tail_addr = i-1;               
                PRINT("find tail!\ntail = %d\n", tail_addr );
                tail_flag =1;
            }  
            if((head_flag == 1)&&(tail_flag == 1))
            {
                PRINT("fin head and tail\n");
                break;
            }            
        }

        len = tail_addr - head_addr+1;        
        PRINT("len = %d\n", len);      
       *pUsbDataLen = len;
        PRINT("pUsbDataLen = %d\n", *pUsbDataLen);
        memcpy(pUsbData, rd_temp+head_addr, len);
        
        //for(i =0; i<len; i++)
        //{
            //printf("pUsbData[%d] = %x\n ", i, *(pUsbData+i)); 
        //}
        //PRINT("-------------\n");        
	}
    else
    {
		PRINT("Receive data error\n");
		return -1;
	}
    if( strncmp((char*)pUsbData, "okok", 4) == 0)
		ret = 0;
	else if (strncmp((char*)pUsbData, "fail", 4) == 0)
		ret = 1;
	else 
		ret = 2;
	PRINT("ret = %d\n", ret);  
	return ret;
}


  
