#!/bin/sh
#
# pcenv.sh -- Run as ". pcenv.sh" to set up Linux PC environment
# for testing of the "wpa2" software on a Linux PC.

WD=`pwd`
export PATH="$WD/install/sbin:$PATH"
export LD_LIBRARY_PATH="$WD/install/lib:$LD_LIBRARY_PATH"

# The following may be helpful for >compiling< for debug mode:
export BUILD_WPA2_DEBUG=y
# Better for valgrind:
export BUILD_WPA2_DEBUG=n
export BUILD_WPA2_STATIC_COMMON_LIBRARY=y
