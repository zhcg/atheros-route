#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.11 2004/09/22 15:26:22 dgregoire Exp
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

include ../../Mk/mk-$(target).mk

OBJDIR=../../objs
OBJ=$(OBJDIR)/net.o $(OBJDIR)/net_rudp.o $(OBJDIR)/net_tcp.o $(OBJDIR)/net_udp.o $(OBJDIR)/net_ka.o $(OBJDIR)/net_cksm.o
BIN=../bin

CFLAGS=-O2 -g -Wall -I../../include -I../../platform/$(target)
CC=gcc

all: $(OBJ) 

$(OBJDIR)/net.o:net.c
	$(CC) $(CFLAGS) -c net.c -o $(OBJDIR)/net.o $(DEFINES)
$(OBJDIR)/net_rudp.o:net_rudp.c
	$(CC) $(CFLAGS) -c net_rudp.c -o $(OBJDIR)/net_rudp.o $(DEFINES)
$(OBJDIR)/net_tcp.o:net_tcp.c
	$(CC) $(CFLAGS) -c net_tcp.c -o $(OBJDIR)/net_tcp.o $(DEFINES)
$(OBJDIR)/net_udp.o:net_udp.c
	$(CC) $(CFLAGS) -c net_udp.c -o $(OBJDIR)/net_udp.o $(DEFINES)
$(OBJDIR)/net_ka.o:net_ka.c
	$(CC) $(CFLAGS) -c net_ka.c -o $(OBJDIR)/net_ka.o $(DEFINES)
$(OBJDIR)/net_cksm.o:net_cksm.c
	$(CC) $(CFLAGS) -c net_cksm.c -o $(OBJDIR)/net_cksm.o $(DEFINES)

clean:
	rm -f $(OBJDIR)/net.o $(OBJDIR)/net_rudp.o $(OBJDIR)/net_tcp.o $(OBJDIR)/net_udp.o $(OBJDIR)/net_ka.o $(OBJDIR)/net_cksm.o    

install: all

