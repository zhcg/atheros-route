#!/bin/sh

/etc/ath/apdown
insmod /lib/modules/2.6.31/net/art-wasp.ko
mknod /dev/dk0 c 63 0
/bin/nart.out -console &
