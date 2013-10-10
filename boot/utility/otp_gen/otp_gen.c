#include <stdio.h>
#include <stdlib.h>

/*
 *        OTP format 
 *
 *        0               1               2               3
 *        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0x10  |          USB info index       |        Patch code Index       |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0x14  | USB bias data | MDIO phy addr |                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *        OTP data 
 *
 *        0               1               2               3
 *        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0x10  |      0x32     |       0x0     |       0x40      |     0x60    |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0x14  | 7 (USB bias)  | 6 (MDIO addr) |        5        |     4       |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

#define OTP_LEADING_WORD    0x0
#define OTP_LEADING_SIZE    32
#define OTP_SIZE            512

#define BITS            8
#define MAX_READ_SZ     80

static unsigned byteIndex =0;
int usb_phy_bias = 5;
int mdio_phy_addr = 5;

static inline int hextodec(char *str)
{
    int i=0; 
    sscanf( str, "%x", &i);
    return i;
}

/*! Pumping the byte in bit
 *
 */
void byte_pump(unsigned char c)
{
    int i=0;
    for(i=0; i<BITS; i++) {
        printf("%d\n", ((c&1<<i)>>i));
    }
    printf("//Byte index:%d=0x%x, value:0x%x=%d\n", byteIndex,byteIndex, c, c);
    byteIndex++;
}


/*! Write a stream of buf with size 
 *
 */
void otp_write_buf(unsigned char *buf, unsigned long size)
{
    int i=0;
   
   // printf("size=%d\n",size);
    for(i=0; i<size; i+=4)
    //for(i=0; i<size/4; i++)
    {
        byte_pump(buf[i+3]);
        byte_pump(buf[i+2]);
        byte_pump(buf[i+1]);
        byte_pump(buf[i]);
    }    
}

/*! Write specific char
 *
 */
void otp_write_char(unsigned char c, unsigned long size)
{
    int i=0;
   
    for(i=0; i<size; i++)
        byte_pump(c);
}

/*! Get file size 
 *
 */
long fsize(FILE *fp)
{
    long sz;
    
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    return sz;
}

/*! Format otp
 *
 * note: 
 *
 * 0. we have to provide a complete 512K bytes data in otp format
 * 1. otp format need 4 0x0 bytes in the first place
 * 2. pad 0x0 depending on the offset
 * 3. pad 0x0 at end of file till otp size
 *
 */
void otp_format(FILE *fp, unsigned int offset)
{
    int size;
    long file_size;
    unsigned char buf[MAX_READ_SZ];

    file_size = fsize(fp);
    
    /* otp need 4 byte leading data */
    otp_write_char(0x0, 0x10);
    
    /* write Usb info index in little endian */
    otp_write_char(96, 1);   //Patch index 2
    otp_write_char(64, 1);   //Patch index 1
    otp_write_char(0,  1);   //USB info index 2
    otp_write_char(offset/2, 1);   //USB info index 1

    /* write USB bias/MDIO phy addr in little endian */
    otp_write_char(4, 1);
    otp_write_char(5, 1);    
    otp_write_char(mdio_phy_addr, 1);    //MDIO phy addr
    otp_write_char(usb_phy_bias, 1);    //USB Phy bias 


    otp_write_char(0x0, ( OTP_LEADING_SIZE-0x10-4-4));

//    printf("offset =%d \n",offset);
    /* pad 0x0 still reach offset */
    if( offset > 0 )
    otp_write_char(0x0, ( offset-OTP_LEADING_SIZE));

 //   printf("data from here\n");
    /* write real data */
    while(1) {
        size = fread(buf, sizeof(unsigned char), sizeof(buf), fp);

        if(size==0) {
            break;
        }
        else if(size<MAX_READ_SZ) {
            otp_write_buf(buf, size);
            break;
        }
        else
            otp_write_buf(buf, MAX_READ_SZ);
    }
    
    /* pad 0x0 till end of OTP_SIZE */
    otp_write_char(0x0, (OTP_SIZE-(offset+file_size)));
}


int main(int argc, char *argv[])
{
    FILE *fp;
    unsigned int offset = 0;

    if( argc != 5 ) 
    {
        printf("Usage: otp_gen usb_phy_bias mdio_phy_addr otp.usb.binary 0x64 > otp.hex\n\r");
        return -1;
    }
    else 
    {
        usb_phy_bias= atoi(argv[1]);
        mdio_phy_addr= atoi(argv[2]);
        offset = hextodec(argv[4]);
    }


    fp = fopen(argv[3],"rb");

    otp_format(fp, offset);

    fclose(fp);

    return 0;
}
