# Makefile for icmpv6proxy -- 
# Version 0.1 beta2 from:
# http://sourceforge.net/projects/igmpproxy
# with minor modifications.

# Shared Makefile stuff, place at BEGINNING of this Makefile
# Makefile.defs needs GWTOP defined!
#GWTOP = ../../..
#include $(GWTOP)/Makefile.defs

prefix=${INSTROOT}
install_bin=$(prefix)/bin
install_etc=$(prefix)/etc
install_lib=$(prefix)/lib
install_template=$(prefix)/template
install_man=$(prefix)/man

# What we build by default:
ALL = icmpv6proxy

# For every C file to be compiled, place it's object file in the OBJS list
# (as required by Makefile.rules).
OBJS = icmpv6main.o igmpsnooping.o config.o confread.o request.o udpsock.o mcgroup.o rttable.o \
	   icmpv6.o ifvc.o callout.o kern.o syslog.o lib.o mroute-api.o fdbtable.o switch-api.o

# Needed libraries (note: .mk files not present during clean!)
#-include $(GWINCLUDE)/libath.mk

# RULES ---------------------------------------------------------------
CC = mips-linux-uclibc-gcc
CFLAGS += -D_GNU_SOURCE -I../ethctl

# Making default targets:
all: build.h local install

build.h	:  
	echo '#define BUILD "' `date +%y%m%d` '"' >build.h

local: $(ALL)

# Making our program:
icmpv6proxy : $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) -g -o $@ $(OBJS) $(LIBS)

# Doing installation (see comments at top of this file)
install: local
	cp -a -f icmpv6proxy $(INSTALL_ROOT)/sbin

# Remove all generated files
clean: default_clean rm_build_h
default_clean:
	rm -rf $(OBJS) icmpv6proxy

rm_build_h:
	rm -rf build.h

# END --------------------------------------------------------------------

# Shared Makefile stuff, place at END of this Makefile
#include $(GWTOP)/Makefile.rules


