#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.5.2.2 2004/02/13 21:44:16 dgregoire Exp
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


include ../Mk/mk-$(target).mk

BIN=../bin

all: tspc.conf.sample 

.PHONY: tspc.conf.sample 
tspc.conf.sample:
	@echo Generating basic configuration file
	@if [ -f $(BIN)/tspc.conf.sample ]; then \
	   rm -f $(BIN)/tspc.conf.sample; \
	fi
	@if [ -z "$(configdir)" ]; then \
	 if [ -z "$(installdir)" ]; then \
	   sed -e "s+@ifname_tun@+$(ifname_tun)+" -e "s+@ifname@+$(ifname)+" -e "s+@target@+$(target)+" -e "s+@tsp_dir@+`cd ..;pwd`+"  tspc.conf.in>$(BIN)/tspc.conf.sample; \
	 else \
	   sed -e "s+@ifname_tun@+$(ifname_tun)+" -e "s+@ifname@+$(ifname)+" -e "s+@target@+$(target)+" -e "s+@tsp_dir@+$(installdir)+" tspc.conf.in >$(BIN)/tspc.conf.sample; \
	 fi \
	else \
	 sed -e "s+@ifname_tun@+$(ifname_tun)+" -e "s+@ifname@+$(ifname)+" -e "s+@target@+$(target)+" -e "s+@tsp_dir@+$(configdir)+" tspc.conf.in >$(BIN)/tspc.conf.sample; \
	fi

	chmod 700 $(BIN)/tspc.conf.sample
	

clean:
	rm -f $(BIN)/tspc.conf.sample

install: all
