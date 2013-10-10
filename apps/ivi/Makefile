# Copyright (c) 2004 HHM IVI Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

#
# $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/ivi/Makefile#1 $
#

prefix=	${INSTROOT}
includedir= ${prefix}/../linux/kernels/mips-linux-2.6.31/include
srcdir=	.
sysconfdir= ${prefix}/etc
localdbdir= /var/local/tmp
user= bin
group= bin

CFLAGS=	-s -I${includedir}
LDFLAGS=
LIBOBJS= 
LIBS=	 
CC=	mips-linux-uclibc-gcc
TARGET=	mroute mroute6

INSTALL=/usr/bin/install -c
INSTALL_PROGRAM=${INSTALL}
INSTALL_DATA=${INSTALL} -m 644
prefix=	${INSTROOT}
exec_prefix=	${prefix}
bindir=	${exec_prefix}/bin
sbindir=${exec_prefix}/sbin
mandir=	${prefix}/man

CLIENTOBJS=	mroute.o
SERVOBJS=	mroute6.o

all:	$(TARGET)
mroute:	$(CLIENTOBJS) $(LIBOBJS)
	$(CC) $(LDFLAGS) -o mroute $(CLIENTOBJS) $(LIBOBJS) $(LIBS)
mroute6: $(SERVOBJS) $(LIBOBJS)
	$(CC) $(LDFLAGS) -o mroute6 $(SERVOBJS) $(LIBOBJS) $(LIBS)

mroute.o:	$(srcdir)/mroute.c
	$(CC) $(CFLAGS) -c $(srcdir)/$*.c
mroute6.o:	$(srcdir)/mroute6.c
	$(CC) $(CFLAGS) -c $(srcdir)/$*.c

install-exec::
	$(INSTALL_PROGRAM) $(TARGET) $(sbindir)
install::
	-mkdir -p $(sbindir) 
	$(INSTALL_PROGRAM) -s -o $(user) -g $(group) $(TARGET) $(sbindir)

includes::

clean::
	/bin/rm -f *.o $(TARGET) $(CLEANFILES)


