#------------------------------------------------------------------------------
# Makefile,v 1.11.2.2 2004/02/13 21:44:15 dgregoire Exp
# 
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
include Mk/mk-$(target).mk

all: pre-checks
	@for dir in ${subdirs}; do \
	  (cd $$dir && $(MAKE) all) \
	  || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac; \
	done && test -z "$$fail"

pre-checks:
	@if [ -z "$(target)" ]; then \
	    echo ; \
	    echo "*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*"; \
	    echo "You must supply the target type!"; \
	    echo "make all target=target-type"; \
	    echo ; \
	    echo "where target-type is one of the following:"; \
	    echo "   freebsd      for FreeBSD (NAT traversal support)."; \
	    echo "   linux        for Linux (NAT traversal support)."; \
	    echo "   windows      for Windows XP/2000 (NAT traversal support)."; \
	    echo "   netbsd       for NetBSD (No NAT support)."; \
            echo "   darwin       for Mac OS X darwin (No NAT support)."; \
	    echo "   solaris      for Solaris (No NAT support)."; \
	    echo "*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*"; \
	    echo ; \
	    exit 1;\
	fi
	@if [ ! -f Mk/mk-$(target).mk ]; then \
	    echo "incomplete target setup for $(target) - no Mk/mk-$(target).mk"; exit 1;\
	fi
	@if [ ! -f template/$(target).sh -a ! -f template/$(target).bat ] ;\
	then \
	    echo "incomplete target setup for $(target) - no template/$(target).sh"; exit 1;\
	fi
	mkdir -p bin
	mkdir -p objs

install: all
	@if [ -z "$(prefix)" ]; then \
	    echo ; \
	    echo "You must specify the install directory"; \
	    echo "ex.: make install target=os-type installdir=/usr/local/tsp"; \
	    echo ; \
	    exit 1;\
	fi
	@INSTALL=cp
	mkdir -p $(prefix)
	@for dir in ${subdirs}; do \
	  (cd $$dir && $(MAKE) install) \
	  || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac; \
	done && test -z "$$fail"

	@mkdir -p $(install_bin)
	@$(COPY) bin/$(TSPC) $(install_bin)

clean: pre-checks
	@for dir in ${subdirs}; do \
          (cd $$dir && $(MAKE) clean) \
          || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac; \
        done && test -z "$$fail"
	/bin/rm -rf bin/tspc objs
