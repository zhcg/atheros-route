#!/bin/bash

# source the variables needed for build
TOP=$1/build
cd $TOP

make BOARD_TYPE=ar7240_emu BUILD_TYPE=jffs2 common_fusion
make BOARD_TYPE=ar7240_emu BUILD_TYPE=jffs2 fusion_build
make BOARD_TYPE=ar7240_emu BUILD_TYPE=ram ram_build

# Nothing special in u-boot for fusion
make BOARD_TYPE=ar7240_emu uboot

echo "---------------------"
find . -name \* -user root -exec sudo chown build {} \; -print
find . -name \.config  -exec chmod 777 {} \; -print
