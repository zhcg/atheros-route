#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h>

//#define USB
//#define UART
//#define SPI
#define PHONE

#define CDD_MAJOR 200
#ifdef USB
#define CDD_MINOR 0
#define DEV		"usb"
#define DEV_NAME "usbpassage"
#endif

#ifdef UART
#define CDD_MINOR 1
#define DEV		"uart"
#define DEV_NAME "uartpassage"
#endif

#ifdef SPI
#define DEV		"spi"
#define DEV_NAME "spipassage"
#define CDD_MINOR 2
#endif

#ifdef PHONE
#define DEV		"phone"
#define DEV_NAME "phonepassage"
#define CDD_MINOR 3
#endif

#define CDD_COUNT 1
#define DATA_BUF_LEN 4096
#define DEV_CLASS_NAME "vd_class"
static u32 cdd_major = 0;

dev_t dev = 0;

typedef struct __flag
{
	int flag;
	int flag_using;
} Open_Flag;

/*定义一个cdev类型的变量*/
static struct cdev cdd_cdev;

static struct class *dev_class = NULL;
static struct device *dev_device = NULL;
static unsigned char* data_buf_local = NULL;
static unsigned char* data_buf_remote = NULL;
static Open_Flag open_flag[2] = {{1,0},{2,0}};
static int i = 0;
//static int wake_flag_local = 0;
//static int wake_flag_remote = 0;
static int data_buf_local_rp;
static int data_buf_local_wp;
static int data_buf_remote_rp;
static int data_buf_remote_wp;
wait_queue_head_t wqh_local;
wait_queue_head_t wqh_remote;

int cdev_open(struct inode *inode, struct file *filp)
{
	if(open_flag[0].flag_using == 1  && open_flag[1].flag_using == 1)
		return -1;
	printk("%s_passage_open\n",DEV);
	if(open_flag[i].flag_using == 1)
	{
		i++;
		if(i==2)
			i = 0;
	}
	filp->private_data = &(open_flag[i].flag);
	open_flag[i].flag_using = 1;
	i++;
	if(i==2)
		i = 0;
	data_buf_local_rp = data_buf_local_wp = data_buf_remote_rp = data_buf_remote_wp = 0;

	return 0;
}
int cdev_read(struct file *filp,  char __user *buf,
			 size_t count, loff_t *offset)
{
	int len;
	unsigned int *data_buf_wp = NULL;
	unsigned int *data_buf_rp = NULL;
	unsigned char *data_buf = NULL;
	wait_queue_head_t *wqh;
	//int *wake_flag_read = NULL;
	unsigned char* data_buf_tmp_read = NULL;

	if (count < 0)
		return -EINVAL;
	if (*(int *)(filp->private_data) == 1) 
	{
		//远端打开读
		//printk("remote read!\n");
		wqh = &wqh_remote;
		//wake_flag_read = &wake_flag_remote;
		data_buf = data_buf_remote;
		data_buf_rp = &data_buf_remote_rp;
		data_buf_wp = &data_buf_local_wp;
	}
	if (*(int *)(filp->private_data) == 2) 
	{
		//本端打开读
		//printk("local read!\n");
		wqh = &wqh_local;
		//wake_flag_read = &wake_flag_local;
		data_buf = data_buf_local;
		data_buf_rp = &data_buf_local_rp;
		data_buf_wp = &data_buf_remote_wp;
	}
	//printk("data_buf_rp = %d\n",*data_buf_rp);
	//printk("data_buf_wp = %d\n",*data_buf_wp);
	//printk("data_buf_remote_rp = %d\n",data_buf_remote_rp);
	//printk("data_buf_local_rp = %d\n",data_buf_local_rp);

	///*定义一个等待队列
     //*将等待队列wq 和当前线程关联起来
	 //*/
	//DECLARE_WAITQUEUE(wq,current);
//
	///*将等待队列wq添加到wqh指向的链表*/
	//add_wait_queue(wqh,&wq);
	///*没有数据供用户空间读*/
	//if(*data_buf_rp != *data_buf_wp)
	//{
		//*data_buf_rp = *data_buf_wp = 0;
//
		///*设置当前线程的状态为睡眠状态*/
		//set_current_state(TASK_INTERRUPTIBLE);
		///*主动让出CPU，重新进行任务调度*/
		//schedule();
		///*等待被唤醒*/	
	//}
	//
	///*从指定的链表中删除等待队列*/
	//remove_wait_queue(wqh,&wq);
	if(*data_buf_rp == *data_buf_wp)
	{
		*data_buf_rp = *data_buf_wp = 0;
		return 0;
	}
	wait_event_interruptible(*wqh,*data_buf_rp != *data_buf_wp);
	//*wake_flag_read = 0;
	if(count > DATA_BUF_LEN)
		count = DATA_BUF_LEN;
	if(*data_buf_rp > *data_buf_wp && count > DATA_BUF_LEN - *data_buf_rp)
	{
		if(count > (DATA_BUF_LEN - *data_buf_rp + *data_buf_wp))
			count = (DATA_BUF_LEN - *data_buf_rp + *data_buf_wp);
		data_buf_tmp_read = (unsigned char *)kmalloc(sizeof(unsigned char) * (count+1), GFP_KERNEL);
		if(data_buf_tmp_read == NULL)
			return -EINVAL;
		len = DATA_BUF_LEN - *data_buf_rp;
		memcpy(data_buf_tmp_read,&data_buf[*data_buf_rp],len);
		memcpy(data_buf_tmp_read+len,data_buf,count - len);
		copy_to_user(buf,data_buf_tmp_read,count);
		kfree(data_buf_tmp_read);
		data_buf_tmp_read = NULL;
		*data_buf_rp = count - len;
		//printk("kernel:read count = %d\n",count);
		return count;
	}
	if(count > *data_buf_wp - *data_buf_rp)
	{
		count = *data_buf_wp - *data_buf_rp;
	}
	//printk("kernel:read count = %d\n",count);
	copy_to_user(buf,&data_buf[*data_buf_rp],count);
	*data_buf_rp += count;
	if(*data_buf_rp > DATA_BUF_LEN)
		*data_buf_rp = 0;
	return count;
}
int cdev_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *offset)
{
	int len;
	unsigned int *data_buf_wp = NULL;
	//unsigned int *data_buf_rp = NULL;
	unsigned char *data_buf = NULL;
	wait_queue_head_t *wqh;
	//int *wake_flag_write = NULL;
	unsigned char* data_buf_tmp_write = NULL;

    if (count < 0)
		return -EINVAL;
	if(count > DATA_BUF_LEN)
		count = DATA_BUF_LEN;
	if (*(int *)(filp->private_data) == 1) 
	{
		//远端打开写
		//printk("remote write!\n");
		wqh = &wqh_local;
		//wake_flag_write = &wake_flag_local;
		data_buf = data_buf_local;
		data_buf_wp = &data_buf_remote_wp;
	}
	if (*(int *)(filp->private_data) == 2) 
	{
		//本端打开写
		//printk("local write!\n");
		wqh = &wqh_remote;
		//wake_flag_write = &wake_flag_remote;
		data_buf = data_buf_remote;
		data_buf_wp = &data_buf_local_wp;
	}

	if(count > DATA_BUF_LEN - *data_buf_wp)
	{
		data_buf_tmp_write = (unsigned char *)kmalloc(sizeof(unsigned char) * (count+1), GFP_KERNEL);
		if(data_buf_tmp_write == NULL)
			return -EINVAL;
		if (copy_from_user(data_buf_tmp_write, buf, count))
		{
			kfree(data_buf_tmp_write);			
			return -EFAULT;			
		}
		len = DATA_BUF_LEN - *data_buf_wp;
		memcpy(&data_buf[*data_buf_wp],data_buf_tmp_write,len);
		*data_buf_wp = 0;
		memcpy(&data_buf[*data_buf_wp],data_buf_tmp_write+len,count - len);
		kfree(data_buf_tmp_write);
		data_buf_tmp_write = NULL;
	}
	else
	{
		if (copy_from_user(&data_buf[*data_buf_wp], buf, count))
			return -EFAULT;
		//printk("\nkernel:write:%s\n",&data_buf[data_buf_wp]);
	}

	*data_buf_wp += count;
	//printk("kernel:write count = %d\n",count);
	//printk("kernel:*data_buf_wp = %d\n",*data_buf_wp);
	//printk("kernel:*data_buf_rp = %d\n",*data_buf_rp);

	if(*data_buf_wp > DATA_BUF_LEN)
		*data_buf_wp = 0;
	//*wake_flag_write = 1;
	wake_up_interruptible(wqh);
	//printk("count = %d\n",count);
	//printk("data_buf_remote_wp = %d\n",data_buf_remote_wp);
	//printk("data_buf_local_wp = %d\n",data_buf_local_wp);
	//printk("data_buf_remote_rp = %d\n",data_buf_remote_rp);
	//printk("data_buf_local_rp = %d\n",data_buf_local_rp);

	return count;
}

int cdev_release(struct inode *inode, struct file *filp)
{	
	int i;
	printk("%s_passage_release\n",DEV);
	for(i=0;i<2;i++)
	{
		if(*(int *)filp->private_data == (open_flag[i].flag))
		{
			open_flag[i].flag_using = 0;
			//printk("release:i = %d\n",i);
		}
	}

	return 0;
}

unsigned int cdev_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	unsigned int *data_buf_wp = NULL;
	unsigned int *data_buf_rp = NULL;
	unsigned char *data_buf = NULL;
	//wait_queue_head_t *wqh;
	//int *wake_flag_poll = NULL;

	/* 判断该设备是否可读写*/
	if (*(int *)(filp->private_data) == 1) 
	{
		//远端打开读
		//printk("remote poll!\n");
		poll_wait(filp, &wqh_remote ,wait);
		//wake_flag_poll = &wake_flag_remote;
		data_buf = data_buf_remote;
		data_buf_rp = &data_buf_remote_rp;
		data_buf_wp = &data_buf_local_wp;
	}
	if (*(int *)(filp->private_data) == 2) 
	{
		//本端打开读
		//printk("local poll!\n");
		poll_wait(filp, &wqh_local ,wait);
		//wake_flag_poll = &wake_flag_local;
		data_buf = data_buf_local;
		data_buf_rp = &data_buf_local_rp;
		data_buf_wp = &data_buf_remote_wp;
	}
	//printk("data_buf_rp = %d\n",*data_buf_rp);
	//printk("data_buf_wp = %d\n",*data_buf_wp);
	if(*data_buf_rp == *data_buf_wp)
	{
		*data_buf_rp = *data_buf_wp = 0;
	}
	//if(*wake_flag_poll!=0)
	if(*data_buf_rp != *data_buf_wp)
	{
		//printk("wake up!!\n");
		mask |= POLLIN | POLLRDNORM;
	}

	/*返回值*/
	return mask;
}
static struct file_operations cdd_fops ={
	.owner = THIS_MODULE,
	.open 	= cdev_open,
	.read	= cdev_read,
	.write 	= cdev_write,
	.release= cdev_release,	
	.poll	= cdev_poll,
};

static int __init virtual_dev_init(void)
{	
	int ret = 0;
	printk("%s_passage_init\n",DEV);

	dev = MKDEV(CDD_MAJOR,CDD_MINOR);
	ret = register_chrdev_region(dev,CDD_COUNT,DEV_NAME);
	if(ret < 0)
	{
		printk("register_chrdev_region failed!\n");
		goto failure_register_chrdev;
	}

	/*获取申请到的主设备号*/
	cdd_major = MAJOR(dev);

	/*初始化cdev*/
	cdev_init(&cdd_cdev, &cdd_fops);
	init_waitqueue_head(&wqh_local);
	init_waitqueue_head(&wqh_remote);

	/*添加cdev到内核中去*/
	ret = cdev_add(&cdd_cdev,dev,CDD_COUNT);
	if(ret < 0)
	{
		printk("cdev_add failed!\n");
		goto failure_cdev_add;
	}

	dev_class = class_create(THIS_MODULE,DEV_CLASS_NAME);
	if(IS_ERR(dev_class))
	{
		printk("class_create failed!\n");
		/*获取错误码*/
		ret = PTR_ERR(dev_class);
		goto failure_class_create;
	}

	dev_device = device_create(dev_class,NULL,dev,
								NULL,DEV_NAME);
	if(IS_ERR(dev_device))
	{
		printk("device_create failed!\n");

		ret = PTR_ERR(dev_device);
		goto failure_device_create;
	}
	
	
	data_buf_local_rp = data_buf_local_wp = data_buf_remote_rp = data_buf_remote_wp = 0;

	data_buf_local = (unsigned char *)kmalloc(sizeof(unsigned char) * DATA_BUF_LEN, GFP_KERNEL);
	data_buf_remote = (unsigned char *)kmalloc(sizeof(unsigned char) * DATA_BUF_LEN, GFP_KERNEL);

	if(data_buf_local == NULL)
		return -1;
	if(data_buf_remote == NULL)
		return -1;

	return 0;
failure_device_create:
	class_destroy(dev_class);
	
failure_class_create:
	cdev_del(&cdd_cdev);	
failure_cdev_add:
	unregister_chrdev_region(dev, CDD_COUNT);
failure_register_chrdev:
	return ret;
}
static void __exit virtual_dev_exit(void)
{
	printk("%s_passage_exit\n",DEV);
	
	data_buf_local_rp = data_buf_local_wp = data_buf_remote_rp = data_buf_remote_wp = 0;

	if(data_buf_local != NULL)
	{
		kfree(data_buf_local);
		data_buf_local = NULL;
	}
	if(data_buf_remote != NULL)
	{
		kfree(data_buf_remote);
		data_buf_remote = NULL;
	}
	
	/*从内核中删除设备*/
	device_destroy(dev_class,dev);

	/*从内核中删除设备类*/
	class_destroy(dev_class);
	
	/*从内核中删除cdev结构*/
	cdev_del(&cdd_cdev);
	/*注销设备号*/
	unregister_chrdev_region(dev,CDD_COUNT);
}

module_init(virtual_dev_init);
module_exit(virtual_dev_exit);
MODULE_LICENSE("GPL");
