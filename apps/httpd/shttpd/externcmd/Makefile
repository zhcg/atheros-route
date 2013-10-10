SRCS=		dev_info.c #cgiext.c
HDRS=		dev_info.h #wlan.h
PROG=		dev_info
export IMAGEPATH=$(INSTROOT)
BINDIR = $(IMAGEPATH)/usr/sbin

# Possible flags: (in brackets are rough numbers for 'gcc -O2' on i386)
# -DHAVE_MD5		- use system md5 library (-2kb)
# -DNDEBUG		- strip off all debug code (-5kb)
# -D_DEBUG		- build debug version (very noisy) (+6kb)
# -DNO_CGI		- disable CGI support (-5kb)
# -DNO_SSL		- disable SSL functionality (-2kb)
# -DNO_AUTH		- disable authorization support (-4kb)
# -DNO_GUI		- Win32 only. Build console version, no GUI
# -DCONFIG=\"file\"	- use `file' as the default config file
# -DNO_SSI		- disable SSI support (-4kb)
# XXX Note for the windows users. In order to build shttpd, MSVS6 is needed.
# Follow these steps:
# 1. Add c:\path_to_msvs6\bin to the system Path environment variable.
# 2. Add two new system environment variables:
#    LIB=c:\path_to_msvs6\lib
#    INCLUDE=c:\path_to_msvs6\include
# 3. start console, go to shttpd-VERSION\src\ directory
# 4. type "nmake msvc"
# 5. go to shttpd-VERSION\examples , type "nmake msvc
#"
export PATH:=$(PATH):$(PWD)/../../../../build/gcc-3.4.4-2.16.1/build_mips/bin
export CC=mips-linux-uclibc-gcc
CROSS=$(subst -gcc,,$(CC))

CFLAGS+= -DEMBEDDED -I$(PWD)/../../../gateway/services/wlan/lib/
LIBS= -ldl -L$(PWD)/../../../gateway/services/wlan/lib -lwlan
OBJS:= $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SRCS)))

.o.c:Makefile
	$(CC) $(CFLAGS) -c $< -o $@

all:$(PROG)
	echo done compiling shttpd

$(PROG):$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(PROG) $(LIBS)
#	$(CC) $(CFLAGS) $(OBJS) compat_unix.c example.c -o $(PROG) $(LIBS)

install:$(PROG) shttpd.pem
	cp $(PROG) $(BINDIR)
	cp shttpd.pem ${IMAGEPATH}/etc
	mkdir -p ${IMAGEPATH}/usr/local/web/
	cp e8.tar.gz ${IMAGEPATH}/usr/local/web/
	cd ${IMAGEPATH}/usr/local/web/&& tar -xzf e8.tar.gz&& rm e8.tar.gz||exit 1

cert:           shttpd.pem

shttpd.pem: shttpd.cnf
	openssl req -new -x509 -days 365 -nodes -config shttpd.cnf -out shttpd.pem -keyout shttpd.pem
	openssl x509 -subject -dates -fingerprint -noout -in shttpd.pem
	chmod 600 shttpd.pem

man:
	cat shttpd.1 | tbl | groff -man -Tascii | col -b > shttpd.1.txt
	cat shttpd.1 | tbl | groff -man -Tascii | less

clean:
	rm -rf *.o *.core $(PROG) lib$(PROG).a
