/*************************************************************************
    > File Name: communication_usb.h
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年12月10日 星期二 13时59分25秒
**************************************************************************/
#ifndef _communication_USB_H_
#define _communication_USB_H_

#include "common_tools.h"
#include "communication_network.h"

#define USB_REG_READ       0x01
#define USB_REG_WRITE      0x02

#define READ_OFFSET		   0x5200
#define WRITE_OFFSET	   0x5000

#define SECTORS 512 // 扇区

#define RAMDEV "/dev/ram1"
#define USBDEV "/dev/usb0"
#define FATFS  "/var/terminal_dev_register/fs/fat_fs"

#define USB_SOCKET_PORT "9685"
struct class_communication_usb
{
    int (* usb_init)();
    int (* usb_send)(int fd, char *buf, int buf_len);
    int (* usb_send_ack)(int fd);
    int (* usb_recv)(int fd, char *buf, int buf_len);
    int (* usb_recv_head)(int fd, char *head, unsigned short head_len);
    int (* usb_exit)();
    
    int fd; // usb描述符
};

/**
 * 
 */
struct s_usb_stub
{
   pthread_mutex_t mutex;
   pthread_cond_t cond;
   
   unsigned char send_allow_flag;
};
extern struct class_communication_usb communication_usb;

#endif
