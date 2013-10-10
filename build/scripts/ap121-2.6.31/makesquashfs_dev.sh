#!/bin/sh

[ $# -ne 2 ] && { echo "Usage: $0 <rootdirpath> <imagename>"; exit 1; };

ROOTDIR=$1
SQUASHFSFILE=$2

rm -rf $ROOTDIR/dev/*
[ $? -ne 0 ] && { echo "rootdir cleanup issue"; exit 1; }

mkdir $ROOTDIR/dev/pts

mknod -m 666 $ROOTDIR/dev/tty c 5 0
mknod -m 666 $ROOTDIR/dev/console c 5 1
mknod -m 666 $ROOTDIR/dev/null c 1 3
mknod -m 666 $ROOTDIR/dev/zero c 1 5
mknod -m 666 $ROOTDIR/dev/ttyS0 c 4 64
mknod -m 666 $ROOTDIR/dev/ttyS1 c 4 65
mknod -m 666 $ROOTDIR/dev/ttyS2 c 4 66
mknod -m 666 $ROOTDIR/dev/mem c 1 1
mknod -m 666 $ROOTDIR/dev/kmem c 1 2
mknod -m 666 $ROOTDIR/dev/random c 1 8
mknod -m 666 $ROOTDIR/dev/urandom c 1 8

mknod -m 666 $ROOTDIR/dev/mtd0 c 90 0
mknod -m 666 $ROOTDIR/dev/mtdr0 c 90 1
mknod -m 666 $ROOTDIR/dev/mtdblock0 b 31 0
mknod -m 666 $ROOTDIR/dev/mtdblock1 b 31 1
mknod -m 666 $ROOTDIR/dev/mtdblock2 b 31 2
mknod -m 666 $ROOTDIR/dev/mtdblock3 b 31 3
mknod -m 666 $ROOTDIR/dev/mtdblock4 b 31 4
mknod -m 666 $ROOTDIR/dev/mtdblock5 b 31 5
mknod -m 666 $ROOTDIR/dev/nvram b 31 4
mknod -m 666 $ROOTDIR/dev/freset c 10 129
mknod -m 666 $ROOTDIR/dev/caldata b 31 3

rm -f $SQUASHFSFILE 
$TOPDIR/build/util/mksquashfs-lzma $ROOTDIR $SQUASHFSFILE -be -info -b 65536 #-nopad
exit 0
