#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.11 2004/09/22 15:26:23 dgregoire Exp
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
OBJ=$(OBJDIR)/tsp_auth.o $(OBJDIR)/tsp_cap.o $(OBJDIR)/tsp_client.o $(OBJDIR)/tsp_net.o $(OBJDIR)/tsp_setup.o 
BIN=../bin

CFLAGS=-O2 -g -Wall -I../../include -I../../platform/$(target)
CC=gcc

all: $(OBJ)

$(OBJDIR)/tsp_auth.o:tsp_auth.c
	$(CC) $(CFLAGS) -c tsp_auth.c -o $(OBJDIR)/tsp_auth.o $(DEFINES)
$(OBJDIR)/tsp_cap.o:tsp_cap.c
	$(CC) $(CFLAGS) -c tsp_cap.c -o $(OBJDIR)/tsp_cap.o $(DEFINES)
$(OBJDIR)/tsp_client.o:tsp_client.c
	$(CC) $(CFLAGS) -c tsp_client.c -o $(OBJDIR)/tsp_client.o $(DEFINES)
$(OBJDIR)/tsp_net.o:tsp_net.c
	$(CC) $(CFLAGS) -c tsp_net.c -o $(OBJDIR)/tsp_net.o $(DEFINES)
$(OBJDIR)/tsp_setup.o:tsp_setup.c
	$(CC) $(CFLAGS) -c tsp_setup.c -o $(OBJDIR)/tsp_setup.o $(DEFINES)

clean:
	rm -f $(OBJDIR)/tsp_auth.o $(OBJDIR)/tsp_cap.o $(OBJDIR)/tsp_client.o $(OBJDIR)/tsp_net.o $(OBJDIR)/tsp_setup.o 

install: all

