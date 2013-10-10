#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.2 2004/02/11 04:52:43 blanchet Exp
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

all:

install: all
	@echo Installing man pages
	mkdir -p $(install_man)/man5
	mkdir -p $(install_man)/man8
	$(COPY) man5/tspc.conf.5 $(install_man)/man5
	$(COPY) man8/tspc.8 $(install_man)/man8
	@echo To view man pages run :
	@echo    man -M $(install_man) tspc
	@echo    man -M $(install_man) tspc.conf

clean: 
	@echo