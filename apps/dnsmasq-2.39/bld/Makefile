ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CFLAGS ?= -O2
ifeq ($(strip $(HAVE_NO_FPU)),y)
CFLAGS += -DHAVE_NO_FPU
endif

PKG_CONFIG ?= pkg-config
CC= $(CROSS)gcc
#LDFLAGS=-static
LDFLAGS=-rdynamic
LDLIBS=-ldl

STRIPCMD = $(CROSS)strip
STRIPFLAGS = --strip-all --remove-section=.note --remove-section=.comment


OBJS = cache.o rfc1035.o util.o option.o forward.o isc.o network.o \
       dnsmasq.o dhcp.o lease.o rfc2131.o netlink.o dbus.o bpf.o \
       helper.o tftp.o log.o captiveportal.o arp.o

.c.o:
	$(CC) $(CFLAGS) $(COPTS) $(I18N) `echo $(COPTS) | ../bld/pkg-wrapper $(PKG_CONFIG) --cflags dbus-1` $(RPM_OPT_FLAGS) -Wall -W -c $<

dnsmasq : $(OBJS)
	$(CC) $(LDFLAGS) -o $@  $(OBJS) `echo $(COPTS) | ../bld/pkg-wrapper $(PKG_CONFIG) --libs dbus-1` $(LIBS) $(LDLIBS) 
	$(STRIPCMD) $(STRIPFLAGS) $@
 
dnsmasq.pot : $(OBJS:.o=.c) dnsmasq.h config.h
	xgettext -d dnsmasq --foreign-user --keyword=_ -o dnsmasq.pot -i $(OBJS:.o=.c)

%.mo : ../po/%.po dnsmasq.pot
	msgmerge -o - ../po/$*.po dnsmasq.pot | msgfmt -o $*.mo -
