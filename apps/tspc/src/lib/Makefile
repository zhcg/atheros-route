#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.11 2004/09/22 15:26:21 dgregoire Exp
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
OBJ=$(OBJDIR)/base64.o $(OBJDIR)/cli.o $(OBJDIR)/config.o $(OBJDIR)/lib.o $(OBJDIR)/log.o $(OBJDIR)/md5c.o $(OBJDIR)/cnfchk.o
BIN=../bin

CFLAGS=-O2 -g -Wall -I../../include -I../../platform/$(target)
CC=gcc

all: $(OBJ) 

$(OBJDIR)/base64.o:base64.c
	$(CC) $(CFLAGS) -c base64.c -o $(OBJDIR)/base64.o $(DEFINES)
$(OBJDIR)/cli.o:cli.c
	$(CC) $(CFLAGS) -c cli.c -o $(OBJDIR)/cli.o $(DEFINES)
$(OBJDIR)/config.o:config.c
	$(CC) $(CFLAGS) -c config.c -o $(OBJDIR)/config.o $(DEFINES)
$(OBJDIR)/lib.o:lib.c
	$(CC) $(CFLAGS) -c lib.c -o $(OBJDIR)/lib.o $(DEFINES)
$(OBJDIR)/log.o:log.c
	$(CC) $(CFLAGS) -c log.c -o $(OBJDIR)/log.o $(DEFINES)
$(OBJDIR)/md5c.o:md5c.c
	$(CC) $(CFLAGS) -c md5c.c -o $(OBJDIR)/md5c.o $(DEFINES)
$(OBJDIR)/cnfchk.o:cnfchk.c
	$(CC) $(CFLAGS) -c cnfchk.c -o $(OBJDIR)/cnfchk.o $(DEFINES)

clean:
	rm -f $(OBJDIR)/base64.o $(OBJDIR)/cli.o $(OBJDIR)/config.o $(OBJDIR)/lib.o $(OBJDIR)/log.o $(OBJDIR)/md5c.o $(OBJDIR)/cnfchk.o

install: all
