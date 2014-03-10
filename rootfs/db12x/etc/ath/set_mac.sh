#!/bin/sh

cfg -a ETH0_MAC=`/usr/sbin/get_mac eth0`
cfg -a ETH0_DFMAC=`/usr/sbin/get_mac eth0`
cfg -a BR0_MAC=`/usr/sbin/get_mac br0`
cfg -c
