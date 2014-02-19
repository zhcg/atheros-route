#
# Makefile for Atheros tools.
#
export PATH = ../../build/gcc-4.3.3/build_mips/staging_dir/usr/bin:$PATH
export TOOLPREFIX=mips-linux-uclibc-
CROSS:=$(TOOLPREFIX)
CC=$(CROSS)gcc
STRIP           =$(CROSS)strip


ppy: ppy.c
	${CC} -o ppy ppy.c

clean: 
	rm -f ppy ppy.o 
