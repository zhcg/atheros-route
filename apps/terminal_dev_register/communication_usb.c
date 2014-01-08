/*************************************************************************
    > File Name: communication_usb.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年12月10日 星期二 13时59分08秒
**************************************************************************/
#include "communication_usb.h"

/**
 * 初始化usb
 */
static int usb_init();

/**
 * usb发送接口
 */
static int usb_send(int fd, char *buf, int buf_len);

/**
 * usb发送响应接口
 */
static int usb_send_ack(int fd);

/**
 * usb接收接口
 */
static int usb_recv(int fd, char *buf, int buf_len);

/**
 * usb资源的释放
 */
static int usb_exit();

struct class_communication_usb communication_usb = 
{
    .usb_init = usb_init,
    .usb_exit = usb_exit,
    .usb_send = usb_send,
    .usb_send_ack = usb_send_ack,
    .usb_recv = usb_recv,
    
    .fd = 0,
};

/**
 * 初始化usb
 */
int usb_init()
{
    int res = 0;
    int fatfs_fd = 0, fd = 0;
    int count = 0;
	char *fs_data = NULL;
	char *data = NULL;

	if ((fd = open(RAMDEV, O_RDWR | O_SYNC)) < 0)
	{
	    PERROR("open %s failed!\n", RAMDEV);
	    return OPEN_ERR;
	}
	
	if ((fatfs_fd = open(FATFS, O_RDONLY)) < 0)
	{
	    PERROR("open %s failed!\n", FATFS);
	    return OPEN_ERR;
	}
	
	if ((fs_data = (char *) malloc(SECTORS)) == NULL)
	{
	    PERROR("malloc failed!\n");
	    return MALLOC_ERR;
    }
    
	while ((res = read (fatfs_fd, fs_data, SECTORS)) > 0) 
	{
		data = fs_data;
		count = res;
		
		do
		{
			if ((res = write(fd, data, count)) > 0)
			{    
			    count -= res;
			    data += res;
			}
		}
		while(count);	
	}
	free (fs_data);
	close(fatfs_fd);
	communication_usb.fd = fd;
    return 0;
}

/**
 * usb发送接口
 */
int usb_send(int fd, char *buf, int buf_len)
{
    int res = 0;
    int i = 0;
    unsigned short len = 0;
    
    #if 1 // 热插拔存在问题
    unsigned int usb_reg = 0;
    int usb_fd = 0;
	if ((usb_fd = open (USBDEV, O_RDWR)) < 0)
	{
	    PERROR("open %s failed!\n", USBDEV);
	    return OPEN_ERR;
	}
	
	if (ioctl(usb_fd, USB_REG_READ, &usb_reg) < 0) 
	{
	    PERROR("open USBDEV failed!\n", USBDEV);
	    close(usb_fd);
	    return IOCTL_ERR;
	}
	PRINT("USB_REG_READ == %08X\n",usb_reg);
	
	if(((usb_reg & 0x80) == 0x80) && ((usb_reg & 0x200) ==  0x200))
	{
	    system("rmmod /lib/modules/2.6.31/usb/g_file_storage.ko");
	    usleep(100 * 1000);
	    system("insmod /lib/modules/2.6.31/usb/g_file_storage.ko file=/dev/ram1 stall=0 removable=1");
    }
    close(usb_fd);
    #endif
    
    // 先读 5s
    for (i = 0; i < 25; i++)
    {
        // 接收响应
        lseek(fd, WRITE_OFFSET, SEEK_SET);
        if ((res = read(fd, &len, sizeof(len))) != sizeof(len))
        {
            usleep(200000);
            continue;
        }
        PRINT("len = %d %04X\n", len, len);
        if (len == 0)
        {
            PRINT("This is a response!\n");
            break;
        }
        usleep(200000);
    }
    if (i == 25)
    {
        PRINT("timeout!\n");
        return TIMEOUT_ERR;
    }
    
    // 发送数据
    lseek(fd, WRITE_OFFSET, SEEK_SET);
    if ((res = write(fd, buf, buf_len)) < 0)
    {
        PERROR("send_data failed!\n");
        return res;
    }
    // 同步到存储设备
    fsync(fd);
    
    return 0;
}

/**
 * usb发送响应接口
 */
int usb_send_ack(int fd)
{
    int res = 0;
    int i = 0;
    char buf[SECTORS] = {0};
    
    #if 1 // 热插拔存在问题
    unsigned int usb_reg = 0;
    int usb_fd = 0;
    PRINT("before open!\n");
	if ((usb_fd = open (USBDEV, O_RDWR)) < 0)
	{
	    PERROR("open %s failed!\n", USBDEV);
	    return OPEN_ERR;
	}
	PRINT("after open!\n");
	if (ioctl(usb_fd, USB_REG_READ, &usb_reg) < 0) 
	{
	    PERROR("open USBDEV failed!\n", USBDEV);
	    close(usb_fd);
	    return IOCTL_ERR;
	}
	PRINT("USB_REG_READ == %08X\n",usb_reg);
	
	if(((usb_reg & 0x80) == 0x80) && ((usb_reg & 0x200) ==  0x200))
	{
	    system("rmmod /lib/modules/2.6.31/usb/g_file_storage.ko");
	    usleep(100 * 1000);
	    system("insmod /lib/modules/2.6.31/usb/g_file_storage.ko file=/dev/ram1 stall=0 removable=1");
    }
    close(usb_fd);
    #endif
    
    // 发送数据
    lseek(fd, READ_OFFSET, SEEK_SET);
    if ((res = write(fd, buf, sizeof(buf))) < 0)
    {
        PERROR("send_data failed!\n");
        return res;
    }
    // 写入到存储设备
    fsync(fd);
    return 0;
}
/**
 * usb接收接口
 */
int usb_recv(int fd, char *buf, int buf_len)
{
    int res = 0;
    short len = 0;
    char recv_buf[SECTORS] = {0};
    
    #if 1 // 热插拔存在问题
    unsigned int usb_reg = 0;
    int usb_fd = 0;
	if ((usb_fd = open (USBDEV, O_RDWR)) < 0)
	{
	    PERROR("open %s failed!\n", USBDEV);
	    return OPEN_ERR;
	}
	
	if (ioctl(usb_fd, USB_REG_READ, &usb_reg) < 0) 
	{
	    PERROR("open USBDEV failed!\n", USBDEV);
	    close(usb_fd);
	    return IOCTL_ERR;
	}
	PRINT("USB_REG_READ == %08X\n",usb_reg);
	
	if(((usb_reg & 0x80) == 0x80) && ((usb_reg & 0x200) ==  0x200))
	{
	    system("rmmod /lib/modules/2.6.31/usb/g_file_storage.ko");
	    usleep(100 * 1000);
	    system("insmod /lib/modules/2.6.31/usb/g_file_storage.ko file=/dev/ram1 stall=0 removable=1");
    }
    close(usb_fd);
    #endif
    
    lseek(fd, READ_OFFSET, SEEK_SET);
    if ((res = read(fd, recv_buf, sizeof(recv_buf))) < 0)
    {
        PERROR("read failed!\n");
        return res;
    }
    
    // 大小端的问题
    memcpy(&len, recv_buf, sizeof(len));
                    
    // 长度最高位为一 说明长度超过510 最高位为0说明
    PRINT("len = %04X %d\n", len, len);
    
    // 正确的数据包取非  
    if ((len < 0) || (len > (SECTORS - 2)))
    {
        PERROR("data err\n");
        return DATA_ERR;
    }
    else if (len == 0) 
    {
        return 0;
    }
    else 
    {
        memcpy(buf, recv_buf + 2, len);
    }
    return len;
}

/**
 * usb资源的释放
 */
int usb_exit()
{
    int res = 0;
    if (communication_usb.fd != 0)
    {
        close(communication_usb.fd);
    }
    return 0;
}
