#GWTOP = ../../gateway
#include $(GWTOP)/Makefile.defs

SRCS=		string.c shttpd.c log.c auth.c md5.c cgi.c io_ssi.c \
		io_file.c io_socket.c io_ssl.c io_emb.c io_dir.c io_cgi.c\
		shttpdext.c ssi_usrman.c  webuserman.c captiveportal.c\
	        ssi_langout.c ssi_modcntrl.c	
HDRS=		defs.h llist.h shttpd.h std_includes.h io.h md5.h ssl.h \
		compat_unix.h compat_win32.h compat_rtems.h config.h wlan.h\
	 	shttpdext.h ssi_usrman.h webuserman.h captiveportal.h ssi_langout.h\
		ssi_modcntrl.h
		
OBJS=		$(SRCS:%.c=%.o)
PROG=		shttpd
CL_FLAGS=	/MD /TC /nologo /DNDEBUG /Os 	# MSVC compiler flags
ifdef INSTALL_ROOT
IMAGEPATH=$(INSTALL_ROOT)
else
IMAGEPATH=../../gateway/install
endif
BINDIR = $(IMAGEPATH)/sbin
#export PATH:=$(PATH):$(PWD)/../../../build/gcc-3.4.4-2.16.1/build_mips/bin:$(PWD)/../../gateway/install/include
#export CC=mips-linux-uclibc-gcc
#AR=mips-linux-uclibc-ar
#LIBS= -ldl -L$(PWD)/../../gateway/services/wlan/lib -lwlan
#LIBS =-ldl -L$(PWD)/../../gateway/install/static_lib
LIBS += -ldl

# CFLAGS+= -DEMBEDDED -DNO_THREADS -I$(PWD)/../../gateway/services/wlan/lib/ -I$(PWD)/../../gateway/services/
ifeq ($(GWLANG),en)
MLANGFLAGS = -DCTC_E8_PAGES
endif

ifeq ($(GWLANG),ch)
MLANGFLAGS = -DCTC_E8_PAGES
endif

ifeq ($(GWLANG),mlang_cn)
MLANGFLAGS = -DMULITYLANG_PAGES
endif

ifeq ($(GWLANG),mlang_en)
MLANGFLAGS = -DMULITYLANG_PAGES
endif

ifeq ($(MLANGFLAGS),)
CPPFLAGS += -DCTC_E8_PAGES
else
CPPFLAGS += $(MLANGFLAGS)
endif

CFLAGS+= -DEMBEDDED -DNO_THREADS -g 
# Possible flags: (in brackets are rough numbers for 'gcc -O2' on i386)
# -DHAVE_MD5		- use system md5 library (-2kb)
# -DNDEBUG		- strip off all debug code (-5kb)
# -D_DEBUG		- build debug version (very noisy) (+6kb)
# -DNO_CGI		- disable CGI support (-5kb)
# -DNO_SSL		- disable SSL functionality (-2kb)
# -DNO_AUTH		- disable authorization support (-4kb)
# -DCONFIG=\"file\"	- use `file' as the default config file
# -DNO_SSI		- disable SSI support (-4kb)
# -DNO_THREADS		- disable threads support (?)

# XXX Note for the windows users. In order to build shttpd, MSVS6 is needed.
# Follow these steps:
# 1. Add c:\path_to_msvs6\bin to the system Path environment variable.
# 2. Add two new system environment variables:
#    LIB=c:\path_to_msvs6\lib
#    INCLUDE=c:\path_to_msvs6\include
# 3. start console, go to shttpd-VERSION\src\ directory
# 4. type "nmake msvc"
# 5. go to shttpd-VERSION\examples , type "nmake msvc"


all:install
	@echo done compiling shttpd

.c.o:
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
$(PROG):lib$(PROG).a
	$(CC) $(CFLAGS) standalone.c -L. -l$(PROG) -o $(PROG) $(LIBS) 	
lib$(PROG).a: $(OBJS) compat_unix.o
	$(AR) -r lib$(PROG).a $(OBJS) compat_unix.o 2>&1

install:$(PROG)
	mkdir -p  $(BINDIR)
	cp $(PROG) $(BINDIR)
#	mkdir -p ${IMAGEPATH}/usr/local/web/
#	cp -f e8.tar.gz ${IMAGEPATH}/usr/local/web/
#	cd ${IMAGEPATH}/usr/local/web/&& tar -xzf e8.tar.gz&& rm -f e8.tar.gz||exit 1
cert:           shttpd.pem

shttpd.pem: shttpd.cnf
	openssl req -new -x509 -days 365 -nodes -config shttpd.cnf -out shttpd.pem -keyout shttpd.pem
	openssl x509 -subject -dates -fingerprint -noout -in shttpd.pem
	chmod 755 shttpd.pem
man:
	cat shttpd.1 | tbl | groff -man -Tascii | col -b > shttpd.1.txt
	cat shttpd.1 | tbl | groff -man -Tascii | less

clean:
	rm -rf *.o *.core $(PROG) lib$(PROG).a *.lib *.obj
