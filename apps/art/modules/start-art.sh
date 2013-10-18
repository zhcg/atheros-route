#!/bin/sh

/etc/ath/apdown
insmod /lib/modules/2.6.31/net/art-wasp-osprey.ko
mknod /dev/dk0 c 63 0
mknod /dev/dk1 c 63 1
/bin/nart.out -instance 0 -port 2390 -console &
/bin/nart.out -instance 1 -port 2391 -console &
