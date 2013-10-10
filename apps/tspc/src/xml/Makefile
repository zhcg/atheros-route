#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.9 2004/09/22 15:26:24 dgregoire Exp
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
OBJ=$(OBJDIR)/xmlparse.o $(OBJDIR)/xml_req.o $(OBJDIR)/xml_tun.o
BIN=../bin

CFLAGS=-O2 -g -Wall -I../../include -I../../platform/$(target)
CC=gcc

all: $(OBJ) 

$(OBJDIR)/xmlparse.o:xmlparse.c
	$(CC) $(CFLAGS) -c xmlparse.c -o $(OBJDIR)/xmlparse.o $(DEFINES)
$(OBJDIR)/xml_req.o:xml_req.c
	$(CC) $(CFLAGS) -c xml_req.c -o $(OBJDIR)/xml_req.o $(DEFINES)
$(OBJDIR)/xml_tun.o:xml_tun.c
	$(CC) $(CFLAGS) -c xml_tun.c -o $(OBJDIR)/xml_tun.o $(DEFINES)

clean:
	rm -f $(OBJDIR)/xmlparse.o $(OBJDIR)/xml_req.o $(OBJDIR)/xml_tun.o

install: all
