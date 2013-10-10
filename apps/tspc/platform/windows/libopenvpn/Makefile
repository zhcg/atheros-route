#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.1 2004/03/12 21:59:16 dgregoire Exp
#
# This source code copyright (c) Hexago Inc. 2002-2004.
#
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License (GPL) Version 2, 
# June 1991 as published by the Free  Software Foundation.
#
# This program is distributed in the hope that it will be useful, 
# but WITHOUT ANY WARRANTY;  without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License 
# along with this program; see the file GPL_LICENSE.txt. If not, write 
# to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
# MA 02111-1307 USA
#
include ../../../Mk/mk-$(target).mk

OBJDIR=../../../objs
OBJ=api.o buffer.o error.o fdmisc.o inet_aton.o io.o misc.o mtu.o proxy.o socket.o tun.o win32.o
BIN=../../../bin

CC=gcc -O2 -Wall -mno-cygwin -Wno-unused-function -Wno-unused-variable -I../../../include -I../../../platform/$(target)

all: $(OBJDIR)/libopenvpn.a

$(OBJDIR)/libopenvpn.a: $(OBJ) 
	ar r $(OBJDIR)/libopenvpn.a $(OBJ)

api.o:api.c
	$(CC) -c api.c $(DEFINES)
buffer.o:buffer.c
	$(CC) -c buffer.c $(DEFINES)
error.o:error.c
	$(CC) -c error.c $(DEFINES)
fdmisc.o:fdmisc.c
	$(CC) -c fdmisc.c $(DEFINES)
inet_aton.o:inet_aton.c
	$(CC) -c inet_aton.c $(DEFINES)
io.o:io.c
	$(CC) -c io.c $(DEFINES)
misc.o:misc.c
	$(CC) -c misc.c $(DEFINES)
mtu.o:mtu.c
	$(CC) -c mtu.c $(DEFINES)
proxy.o:proxy.c
	$(CC) -c proxy.c $(DEFINES)
socket.o:socket.c
	$(CC) -c socket.c $(DEFINES)
tun.o:tun.c
	$(CC) -c tun.c $(DEFINES)
win32.o:win32.c
	$(CC) -c win32.c $(DEFINES)

clean:
	rm -f $(OBJ) $(OBJDIR)/libopenvpn.a

install: all
