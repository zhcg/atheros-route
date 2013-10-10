#!/bin/bash

# source the variables needed for build
TOP=$1/build
cd $TOP

#
#echo "---------------------"
#echo "Resetting permissions"
#echo "---------------------"
#find . -name \* -user root -exec sudo chown build {} \; -print 
#find . -name \.config  -exec chmod 777 {} \; -print 
#
make BOARD_TYPE=ap96-small
