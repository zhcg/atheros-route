#include "file.h"
#include "common.h"

/* CRC 高位字节值表 */
static const unsigned char auchCRCHi[] = {
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
} ;
/* CRC低位字节值表*/
static const unsigned char auchCRCLo[] = 
{
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
	0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
	0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
	0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
	0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
	0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
	0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
	0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
	0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
	0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
	0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
	0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
	0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
	0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
	0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
	0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
	0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
	0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
	0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
	0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
	0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
	0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
	0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
	0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
	0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
	0x43, 0x83, 0x41, 0x81, 0x80, 0x40
} ;

#define HEAD_LEN 8
#define VERSION_INFO_LEN  4
#define VERDION_MAJ_LEN 1  //major version len主版本
#define VERSION_MIN_LEN 1  //minor version len此版本
#define VERSION_SUB_LEN 2 //subordinate version len子版本号
#define BIT_LEN 1
#define CRC_LEN  2
#define BINARY_DATA_LEN  4
#define FILE_LEN  128
#define FILE_CONST_PART_LEN  HEAD_LEN + VERSION_INFO_LEN + CRC_LEN + BINARY_DATA_LEN 

//const unsigned char fileHead[HEAD_LEN] = {0x41H,0x70H, 0x70H, 0x42H, 0x69H, 0x6EH, 0x20H,0x20H};

/*此Version的值由version函数调用后传入
typedef struct _strVersion
{
   const unsigned char majorVersion ;
   const unsigned char minorVersion ;
   unsigned char subVersion[VERSION_SUB_LEN]; 
}strVersion;
*/
typedef struct _stFile
{
   int fd; //文件描述符
   int fileLenInByte; //文件实际的大小  
   unsigned char *fileBuf; //文件buffer
}stFile;

unsigned short CRC16(unsigned char *puchMsg, unsigned int usDataLen)
{
	unsigned char uchCRCHi = 0xFF ; /* 高CRC字节初始化 */
	unsigned char uchCRCLo = 0xFF ; /* 低CRC 字节初始化 */
	unsigned uIndex ; 		/* CRC循环中的索引 */
	while (usDataLen--) 	/* 传输消息缓冲区 */
	{
		uIndex = uchCRCHi ^ *(puchMsg++); /* 计算CRC */
		uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
		uchCRCLo = auchCRCLo[uIndex] ;
	}
	return (uchCRCHi << 8 | uchCRCLo) ;
}

/*
name: int version(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
fun: 读取版本号  
para:  struct UKey *pUKey  :IN USB控制相关结构体
       unsigned char *pUsbData: OUT读取USB的数据指针 eg 输出数据为: 1111 
       int *pUsbDataLen: OUT 读取的USB数据的长度  eg:4
ret: 0，成功； 其他值:失败
other: eg:版本号的规则0xA50x5A11110xA50x5A

int version(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen)
{
    
}
*/

/*
功能: 获得文件的大小 
返回值: -1, 获取文件大小失败； >0:返回文件大小

unsigned long getFileSize(const char * path)  
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if(stat(path, &statbuff) < 0){  
        return filesize;  
    }else{  
        filesize = statbuff.st_size;  
    }  
    return filesize;  
}  
*/

/*
name:  int check_file(strVersion *pVersion,  stFile *pComingFile)
fun:   通过检查文件的固定的格式和文件中二进制数据的CRC校验， 来检查文件是否正确。并检查文件是否需要升级。
para:  char *pVersion IN   版本号, eg:1256  1为主版本号, 2为此版本号,  56为子版本号
       unsigned char *pPath  IN OUT 升级文件的路径
ret:  1-5 文件错误; 6文件传输正确，但不需要升级; 7文件传输正确，需要升级
other: 文件错误详细信息: 
      1:文件长度错误;   2:文件头错误; 3: 主版本号错误;  4 : 次版本号错误 ; 5: 二进制文件错误       
*/
int check_file(char *pVersion, const unsigned char *pPath)
{
    int ret =0;
    int i = 0 ;
    int comingBinLen = 0; //存放升级文件中二进制数据的长度
    unsigned char arrbinaryLen[BINARY_DATA_LEN] = {0};
    int fileLen = 0; //按照文件中文件的各个部分计算出来的文件长度
    unsigned short CRCVal = 0; //重新计算的文件的CRC验证值
    unsigned char arrCRC[CRC_LEN] = {0};
    int comingCRCVal = 0; //文件中的CRC 校验转化成的十进制数据 
    int fd = 0;
    stFile comingFile;
    comingFile.fd = -1;
    comingFile.fileLenInByte = 0;
    comingFile.fileBuf = NULL;
    int temp =0;
    const unsigned char fileHead[HEAD_LEN] = {0x41,0x70, 0x70, 0x42, 0x69, 0x6E, 0x20,0x20};
    unsigned long filesize = -1;      
    struct stat statbuff; 
    fd= open(pPath, O_RDONLY);
    if(fd == -1)
    {
        return -1;
    }
    comingFile.fd = fd;
    if(fstat(fd, &statbuff) < 0)
    {  
         comingFile.fileLenInByte = -1;  
         PRINT("***get coming file length fail! \n");
         return -2;
    }
    else
    {  
        comingFile.fileLenInByte = statbuff.st_size;       
    }  
    PRINT("file len = %d\n", comingFile.fileLenInByte);
    if(comingFile.fileLenInByte > 0)
    {
		comingFile.fileBuf = (unsigned char *)malloc(comingFile.fileLenInByte + FILE_LEN);
		if(comingFile.fileBuf == NULL)
		{
			PRINT("malloc fail! \n");
			return -3;
		}
	}
	else
	{
		PRINT("file len error\n");
		return -4;
	}
	memset(comingFile.fileBuf,0,comingFile.fileLenInByte + FILE_LEN);
    lseek(comingFile.fd, 0, SEEK_SET);
    int read_len = 0;
    while(1)
    {
		ret= read(comingFile.fd, comingFile.fileBuf+read_len, FILE_LEN);
		if(ret < 0)
		{
			PRINT("***read coming file fail! \n"); 
			free(comingFile.fileBuf);
			return -5;
		}
		read_len += ret;
		if(read_len >= comingFile.fileLenInByte)
		{
			PRINT("read file done %d!\n",read_len);
			break;
		}
	}
	//ret= read(comingFile.fd, comingFile.fileBuf, comingFile.fileLenInByte);
    close(comingFile.fd);
    //for(i= 0; i<FILE_CONST_PART_LEN; i++)
    //{
        //PRINT("buf[%d] = %x\n", i, comingFile.fileBuf[i]);
    //}
   //比较文件中定义的数据长度和实际上获得的文件长度，验证文件长度是否改变
    memcpy(arrbinaryLen, comingFile.fileBuf + HEAD_LEN + VERSION_INFO_LEN +CRC_LEN, BINARY_DATA_LEN);    
   /*
    for(i = 0; i< BINARY_DATA_LEN; i++)
    {
        PRINT("arr[%d] = %x\n", i, arrbinaryLen[i]);
        temp = (int)arrbinaryLen[i] ;
        PRINT("%d\n", temp);
        temp = temp <<8;
        
        PRINT("len = %d\n", comingBinLen);
        comingBinLen = (( &&0xff)<< (8 * (BINARY_DATA_LEN -i -1))) + comingBinLen;
    }*/

//    for(i = 0; i< BINARY_DATA_LEN; i++)
//		PRINT(arrbina);
	//printf("%d\n",arrbinaryLen[0]);
	//printf("%d\n",arrbinaryLen[1]);
	//printf("%d\n",arrbinaryLen[2]);
	//printf("%d\n",arrbinaryLen[3]);
    comingBinLen = arrbinaryLen[0]*256*256*256+arrbinaryLen[1]*256*256+arrbinaryLen[2]*256+arrbinaryLen[3];
    PRINT("len = %d\n", comingBinLen);
    fileLen = FILE_CONST_PART_LEN +comingBinLen;
    PRINT("fileLen = %d\n", fileLen);
    if(comingFile.fileLenInByte != fileLen)
    {
        PRINT("**coming file's length have an error\n");
		free(comingFile.fileBuf);
        return -6;
    }
   // pComingFile->fileLenInByte = fileLen;
    //验证文件头
    if (strncmp(comingFile.fileBuf,fileHead , HEAD_LEN) != 0) 
    {
        PRINT("**coming file's head have an error\n");
		free(comingFile.fileBuf);
        return -7;
    }
    //验证主版本号
    if (strncmp(comingFile.fileBuf + HEAD_LEN, pVersion, VERDION_MAJ_LEN) != 0) 
    {
        PRINT("**coming file's majorVersion have error");
        free(comingFile.fileBuf);
        return -8;
    }
    //验证次版本号
    if (strncmp(comingFile.fileBuf + HEAD_LEN + VERDION_MAJ_LEN,  pVersion + VERDION_MAJ_LEN , VERSION_MIN_LEN) != 0) 
    {
        PRINT("**coming file's minorVersion have error");
		free(comingFile.fileBuf);
        return -9;
    }
	//for(i=0; i<10; i++)
	//{
		//PRINT("%x\n", comingFile.fileBuf[ FILE_CONST_PART_LEN+i]);
	//}
   	//for(i=0; i<50; i++)
	//{
//		PRINT("%x\n", comingFile.fileBuf[ comingFile.fileLenInByte-50+i]);
//	}
//	 PRINT("%d\n", comingFile.fileLenInByte - FILE_CONST_PART_LEN);
  
  // PRINT("%d\n", temp);
   //重新校验CRC
    //CRCVal = CRC16(comingFile.fileBuf + FILE_CONST_PART_LEN-4, comingFile.fileLenInByte - FILE_CONST_PART_LEN+4);
    CRCVal = CRC16(comingFile.fileBuf + FILE_CONST_PART_LEN-4,fileLen-14);
    memcpy(arrCRC, comingFile.fileBuf +HEAD_LEN + VERSION_INFO_LEN, CRC_LEN );
   /* for(i = 0; i<  CRC_LEN; i++)
    {
        comingCRCVal= arrCRC[i] << (8 * (CRC_LEN -i -1)) + comingCRCVal;
    }*/
	printf("%x\n",arrCRC[0]);
	printf("%x\n",arrCRC[1]);
    comingCRCVal = arrCRC[0]*256+arrCRC[1];
	PRINT("--%x\n", comingCRCVal);

	PRINT("--%x\n", CRCVal);
    if(comingCRCVal != CRCVal)
    {
        PRINT("**coming file's binary data have changed\n");
		free(comingFile.fileBuf);
        return -10;
    }
    //删除源文件，并把数据重新写入(只写二进制文件)
    fd =open(pPath, O_RDWR|O_TRUNC);
    if(fd == -1)
    {
        PRINT("**open error");
		free(comingFile.fileBuf);
        return -11;
    }
    else
    {
        comingFile.fd = fd;
    }
    lseek(comingFile.fd, 0, SEEK_SET);
    int write_len = 0;
    while(1)
    {
		ret = write(fd, comingFile.fileBuf + FILE_CONST_PART_LEN + write_len, FILE_LEN);
		if(ret < 0)
		{
			PRINT("**rewrite file have an error\n ");
			free(comingFile.fileBuf);
			return -12;
		}
		write_len += ret;
		if((comingFile.fileLenInByte - write_len) <= FILE_LEN)
		{
			write(fd, comingFile.fileBuf + FILE_CONST_PART_LEN + write_len, comingFile.fileLenInByte - write_len);
			PRINT("write file done!\n");
			break;
		}
	}
    close(fd);
    //验证子版本号 
      
    int new_ver = (*(comingFile.fileBuf + HEAD_LEN + VERDION_MAJ_LEN + VERSION_MIN_LEN))*256+ (*(comingFile.fileBuf + HEAD_LEN + VERDION_MAJ_LEN + VERSION_MIN_LEN+1));
    int old_ver = (*(pVersion + VERDION_MAJ_LEN +VERSION_MIN_LEN))*256 + (*(pVersion + VERDION_MAJ_LEN +VERSION_MIN_LEN+1));
    PRINT("new_ver = %d\n",new_ver);
    PRINT("old_ver = %d\n",old_ver);
    if (new_ver <= old_ver) 
    {
        PRINT("**coming file's don't need upgrade\n");        
		free(comingFile.fileBuf);
        return -13;
    }
    else
    {
        PRINT("-local software need upgrade\n");
		free(comingFile.fileBuf);
        return 0;
    }    
}
//
//const unsigned char  comingFilePath[100] = DATNAME; //文件的路径
//
//int main(int argc, char* argv[])
//{
    //unsigned short ret = 0;
    //char version[4]= {0x01, 0x01, 0x00, 0x13};
    //PRINT("-------\n");
    //ret=check_file(version, comingFilePath);
    //PRINT("ret = %d\n", ret);
//}





