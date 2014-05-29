#
# Makefile for SLIC Data integrity check Driver
#
obj-m	+=	ath_si32178.o

KDIR :=/home/zhangbo/9344_lite/9344_lite/linux/kernels/mips-linux-2.6.31
#KDIR =  /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)                  

all:
	make -C	$(KDIR)	SUBDIRS=$(PWD)	modules 

clean:
	rm -rf *.o *.ko *.mod.* .*.cmd Module.* *.order
