#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.2 2004/06/15 17:43:44 dgregoire Exp
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

OBJ=win-ver.o
BIN=../../../bin

CC=gcc -O2 -Wall -mno-cygwin -Wno-unused-function -Wno-unused-variable -I../../../include -I../../../platform/$(target)

all: win-ver.exe

win-ver.exe: win-ver.o
	$(CC) -o win-ver.exe win-ver.o 

win-ver.o: win-ver.c
	$(CC) -c win-ver.c $(DEFINES)

clean:
	rm -f win-ver.o win-ver.exe ../nsis-installer-code/template/win-ver.exe

install: all
	mkdir -p ../nsis-installer-code/template
	strip win-ver.exe
	cp win-ver.exe ../nsis-installer-code/template

