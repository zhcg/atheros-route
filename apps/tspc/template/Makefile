#----- Makefile --------------------------------------------------------------------------------
#
# Makefile,v 1.7 2004/04/05 21:41:52 dgregoire Exp
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
	@echo Installing templates
	@if [ $(template) ]; then ( \
		mkdir -p ../$(install_template); \
		for template_var in README checktunnel.bat $(template); do \
	   	$(COPY) $${template_var} ../$(install_template)/$${template_var}; \
		done; ) \
	else ( \
		mkdir -p $(install_template); \
		for template_var in README checktunnel.sh $(target).sh; do \
                $(COPY) $${template_var} $(install_template)/$${template_var}; \
                done; ) \
	fi

clean: 
	@echo
