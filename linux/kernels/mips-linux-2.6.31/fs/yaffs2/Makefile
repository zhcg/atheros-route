# Main Makefile for YAFFS
#
#
# YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
#
# Copyright (C) 2002-2007 Aleph One Ltd.
#   for Toby Churchill Ltd and Brightstar Engineering
#
# Created by Charles Manning <charles@aleph1.co.uk>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

obj-$(CONFIG_YAFFS_FS) := yaffs2.o

yaffs2-y := yaffs_mtdif.o yaffs_mtdif2.o
yaffs2-y += yaffs_mtdif1.o yaffs_packedtags1.o
yaffs2-y += yaffs_ecc.o yaffs_fs.o yaffs_guts.o
yaffs2-y += yaffs_packedtags2.o yaffs_qsort.o
yaffs2-y += yaffs_tagscompat.o yaffs_tagsvalidity.o
yaffs2-y += yaffs_checkptrw.o yaffs_nand.o

