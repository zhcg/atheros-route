#!/bin/bash

# source the variables needed for build
TOP=$1/build
cd $TOP

# To make use of the optimized cleanup, first
# clean up the old toolchain
make BOARD_TYPE=ap91-2MB BUILD_TYPE=squashfs toolchain_clean

# Generate a normal build
make BOARD_TYPE=ap91-2MB BUILD_TYPE=squashfs

# make BOARD_TYPE=ap91-2MB BUILD_TYPE=squashfs iptables_build
# make BOARD_TYPE=ap91-2MB BUILD_TYPE=squashfs fusion_build
# make BOARD_TYPE=ap91-2MB BUILD_TYPE=ram ram_build

# Nothing special in u-boot for fusion
# make BOARD_TYPE=ap91-2MB uboot

# Generate NAT/Firewall image for AP91
make BOARD_TYPE=ap91-2MB BUILD_TYPE=squashfs BUILD_CONFIG=_routing

# Delete the optimized toolchain, normal toolchain
# will be built as a part of the next board
make BOARD_TYPE=ap91-2MB BUILD_TYPE=squashfs toolchain_clean

echo "---------------------"
find . -name \* -user root -exec sudo chown build {} \; -print
find . -name \.config  -exec chmod 777 {} \; -print
