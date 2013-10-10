#!/bin/bash

# source the variables needed for build
TOP=$1/build
cd $TOP

make BOARD_TYPE=ap101 BUILD_TYPE=jffs2 common_fusion
make BOARD_TYPE=ap101 BUILD_TYPE=jffs2 fusion_build
make BOARD_TYPE=ap101 BUILD_TYPE=ram ram_build

# Nothing special in u-boot for fusion
make BOARD_TYPE=ap101 uboot

# Generate SAMBA image for AP101
#make BOARD_TYPE=ap101 BUILD_TYPE=squashfs BUILD_CONFIG=_samba

# Generate NAT/Firewall image for AP101
make BOARD_TYPE=ap101 BUILD_TYPE=jffs2 BUILD_CONFIG=_routing


echo "---------------------"
find . -name \* -user root -exec sudo chown build {} \; -print
find . -name \.config  -exec chmod 777 {} \; -print
