KDIR	:= $(KERNELPATH)
PWD	:= $(shell pwd)
ROOTDIR := $(PWD)/modules
ARC 	:=arm 
CROSS_CC :=armeb-linux-
export KDIR
export ROOTDIR
export CROSS_CC

all: default

default:
	make ARCH=$(ARC) CROSS_COMPILE=$(CROSS_CC) -C $(KDIR) M=$(PWD)/modules modules

clean:
	cd modules && make clean
