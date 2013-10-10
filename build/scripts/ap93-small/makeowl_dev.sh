#!/bin/bash

# source the variables needed for build
TOP=$1/build
cd $TOP

make common_fusion BOARD_TYPE=ap93fus

echo "---------------------"
echo "Resetting permissions"
echo "---------------------"
find . -name \* -user root -exec sudo chown build {} \; -print
find . -name \.config  -exec chmod 777 {} \; -print

make fusion_build BOARD_TYPE=ap93fus
