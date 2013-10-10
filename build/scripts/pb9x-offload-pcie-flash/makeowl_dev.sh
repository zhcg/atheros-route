#!/bin/bash

# source the variables needed for build
TOP=$1
BUILD=$TOP/build

TARGET=ub12x-offload
HOST=pb9x-offload

PKG=$TOP/pkg/$HOST/src

if [ "_$2" = "_" ]; then
    BUILDREV="ce_sw_1.0"
else
    BUILDREV=$2
fi

mkdir -p $PKG

cd $BUILD

make BOARD_TYPE=${HOST} SDK_NAME=bsp-sdk_${BUILDREV} PKG_DIR=$PKG pkg_sdk

make BOARD_TYPE=${TARGET}
make BOARD_TYPE=${TARGET} WLAN_NAME=tgt-sdk_${BUILDREV} PKG_DIR=$PKG pkg_wlan

make BOARD_TYPE=${HOST} BUILD_TYPE=jffs2
make BOARD_TYPE=${HOST} WLAN_NAME=hst-sdk_${BUILDREV} PKG_DIR=$PKG pkg_wlan
