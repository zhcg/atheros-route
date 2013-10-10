# Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
# 2003, 2004, 2005  Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

export CC = $(CROSS)gcc
export CFLAGS = -O2  -Wall -Werror -fno-strict-aliasing
export DEFS = -DHAVE_CONFIG_H
export EXEEXT = 
export OBJEXT = o
export RANLIB = $(CROSS)ranlib
export STRIP = $(CROSS)strip
export ARFLAGS = cru
export AR = $(CROSS)ar

srcdir = common \
		 dhcpctl \
		 dst	\
		 minires \
		 omapip \
		 server

top_srcdir = .


all:
	for dir in $(srcdir) ; do (cd $$dir && $(MAKE) all) || exit 1; done

install: 
	$(MAKE) install  -C server

clean: 
	for dir in $(srcdir)  ; do (cd $$dir && $(MAKE) clean) || exit 1; done


.PHONY: all clean install
	


#
# automake adds dependencies that we don't like, so we explicitly remove them
#
Makefile:
# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
