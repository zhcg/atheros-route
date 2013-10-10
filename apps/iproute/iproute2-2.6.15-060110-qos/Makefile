DESTDIR=$(PREFIX)
SBINDIR=/usr/sbin
CONFDIR=/etc/iproute2
DOCDIR=/usr/share/doc/iproute2
MANDIR=/usr/share/man

# Path to kernel include
ifndef KERNEL_DIR
KERNEL_DIR=/usr/src/linux
endif
KERNEL_INCLUDE=$(KERNEL_DIR)/include

# Path to db_185.h include
DBM_INCLUDE:=/usr/include

DEFINES= -DRESOLVE_HOSTNAMES

#options if you have a bind>=4.9.4 libresolv (or, maybe, glibc)
#LDLIBS=-lresolv
ADDLIB=

#options for decnet
ADDLIB+=dnet_ntop.o dnet_pton.o

#options for ipx
ADDLIB+=ipx_ntop.o ipx_pton.o

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC              =$(CROSS)gcc
AR              =$(CROSS)ar
HOSTCC          =$(CC)
STRIP           =$(CROSS)strip
LD              =$(CROSS)ld

CCOPTS = -D_GNU_SOURCE -O2 -Wstrict-prototypes -Wall
CFLAGS = $(CCOPTS) -I../include $(DEFINES)
YACCFLAGS = -d -t -v

LDLIBS += -L../lib -lnetlink -lutil

SUBDIRS=lib ip tc
#SUBDIRS+=misc

LIBNETLINK=../lib/libnetlink.a ../lib/libutil.a

all: Config
	@for i in $(SUBDIRS); \
	do $(MAKE) $(MFLAGS) -C $$i; done

Config:
	sh configure $(KERNEL_INCLUDE)

install: all
	install -m 0755 -d $(DESTDIR)$(SBINDIR)
	install -m 0755 -d $(DESTDIR)$(CONFDIR)
#	install -m 0755 -d $(DESTDIR)$(DOCDIR)/examples
#	install -m 0755 -d $(DESTDIR)$(DOCDIR)/examples/diffserv
#	install -m 0644 README.iproute2+tc $(shell find examples -maxdepth 1 -type f) \
#		$(DESTDIR)$(DOCDIR)/examples
#	install -m 0644 $(shell find examples/diffserv -maxdepth 1 -type f) \
#		$(DESTDIR)$(DOCDIR)/examples/diffserv
	@for i in $(SUBDIRS); do $(MAKE) -C $$i install; done
	install -m 0644 $(shell find etc/iproute2 -maxdepth 1 -type f) $(DESTDIR)$(CONFDIR)
#	install -m 0755 -d $(DESTDIR)$(MANDIR)/man8
#	install -m 0644 $(shell find man/man8 -maxdepth 1 -type f) $(DESTDIR)$(MANDIR)/man8
#	ln -sf tc-pbfifo.8  $(DESTDIR)$(MANDIR)/man8/tc-bfifo.8
#	ln -sf tc-pbfifo.8  $(DESTDIR)$(MANDIR)/man8/tc-pfifo.8
#	install -m 0755 -d $(DESTDIR)$(MANDIR)/man3
#	install -m 0644 $(shell find man/man3 -maxdepth 1 -type f) $(DESTDIR)$(MANDIR)/man3

clean:
	rm -f cscope.*
	@for i in $(SUBDIRS) doc; \
	do $(MAKE) $(MFLAGS) -C $$i clean; done

clobber: clean
	rm -f Config

distclean: clobber

cscope:
	cscope -b -q -R -Iinclude -sip -slib -smisc -snetem -stc

.EXPORT_ALL_VARIABLES:
