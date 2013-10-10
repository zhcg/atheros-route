# Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
# 2003, 2004, 2005  Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


srcdir = .
top_srcdir = ..

INCLUDES = -I. -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/includes \
		   -DLOCALSTATEDIR='"/var"'

COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
LINK = $(CC) $(CFLAGS) -o $@

dhcpd_OBJECTS = dhcpd.$(OBJEXT) dhcp.$(OBJEXT) bootp.$(OBJEXT) \
	confpars.$(OBJEXT) db.$(OBJEXT) class.$(OBJEXT) \
	failover.$(OBJEXT) omapi.$(OBJEXT) mdb.$(OBJEXT) \
	stables.$(OBJEXT) salloc.$(OBJEXT) ddns.$(OBJEXT) \
	dhcpleasequery.$(OBJEXT) dhcpv6.$(OBJEXT) mdb6.$(OBJEXT) arp.$(OBJEXT)


# libomapi.a this is here twice to handle circular library dependencies :(
dhcpd_LDADD = ../common/libdhcp.a  ../omapip/libomapi.a ../dst/libdst.a \
	      ../dhcpctl/libdhcpctl.a ../minires/libres.a \
	      ../omapip/libomapi.a

all:dhcpd$(EXEEXT) 

dhcpd$(EXEEXT): $(dhcpd_OBJECTS) $(dhcpd_DEPENDENCIES) 
	@rm -f dhcpd$(EXEEXT)
	$(LINK) $(dhcpd_OBJECTS) $(dhcpd_LDADD)

.c.o:
	$(COMPILE) -c $<

install: 
	$(STRIP) dhcpd
	cp dhcpd $(PREFIX)/

clean: 
	-rm *.o
	-rm dhcpd


.PHONY: all clean install 

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
