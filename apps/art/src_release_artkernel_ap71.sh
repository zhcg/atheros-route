#!/bin/sh

if [ $# != 1 ]; then
echo "usage: src_release_artkernel_ap71.sh [path] "
exit 1
fi

export TOP_PATH=`pwd`/
export ART_KERNEL_PATH=$1 
export MODULE_PATH=$ART_KERNEL_PATH/modules/ 
export MODULE_INCLUDE_PATH=$MODULE_PATH/include/ 

mkdir -p $MODULE_INCLUDE_PATH/

cd $TOP_PATH/$ART_KERNEL_PATH/
cp $TOP_PATH/Makefile .
cp $TOP_PATH/makefile.soc.linux.mips .


cd $TOP_PATH/$MODULE_PATH/
cp $TOP_PATH/modules/Makefile .
cp $TOP_PATH/modules/main.c .
cp $TOP_PATH/modules/client.c .
cp $TOP_PATH/modules/dk_flash.c .
cp $TOP_PATH/modules/dk_isr.c .
cp $TOP_PATH/modules/dk_event.c .
cp $TOP_PATH/modules/dk_func.c .
cp $TOP_PATH/modules/dk_pci_bus.c .

cd $TOP_PATH/$MODULE_INCLUDE_PATH/
cp $TOP_PATH/modules/include/client.h .
cp $TOP_PATH/modules/include/dk_flash.h .
cp $TOP_PATH/modules/include/dk_event.h .
cp $TOP_PATH/modules/include/dk.h .
cp $TOP_PATH/modules/include/dk_ioctl.h .
cp $TOP_PATH/modules/include/dk_pci_bus.h .
cp $TOP_PATH/modules/include/flbase.h .
cp $TOP_PATH/modules/include/flbuffer.h .
cp $TOP_PATH/modules/include/flcustom.h .
cp $TOP_PATH/modules/include/flflash.h .
cp $TOP_PATH/modules/include/flsocket.h .
cp $TOP_PATH/modules/include/flsystem.h .

cd $TOP_PATH/
