# Makefile,v 1.6 2004/04/20 15:10:56 parent Exp
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

SUBDIRS=libopenvpn win-ver
MAKE=make
LIBS=-liphlpapi -lws2_32

all: tspc.exe
	
tspc.exe: tsp_local.o
	@for dir in ${SUBDIRS}; do \
		(cd $$dir && $(MAKE) all); done
	gcc -O -g -I../../include -Ilibopenvpn -g -o tspc tsp_local.o ../../objs/*.o ../../objs/libopenvpn.a -lws2_32 -liphlpapi

tsp_local.o: tsp_local.c
	gcc -O -g -c -I../../include -Ilibopenvpn -g tsp_local.c

install: all
	@for dir in ${SUBDIRS}; do \
		(cd $$dir && $(MAKE) install); done
	cp tspc.exe ../../bin
	strip ../../bin/tspc.exe

clean:
	@for dir in ${SUBDIRS}; do \
		(cd $$dir && $(MAKE) clean); done
	rm -f tspc.exe ../../bin/tspc.exe


