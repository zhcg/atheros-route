#------------------------------------------------------------------------------
# mk-solaris.mk,v 1.2 2004/03/30 15:34:17 jfboud Exp
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
# Solaris8 particulars.

DEFINES=-Dsolaris
COPY=cp
TSPC=tspc
install_bin=$(installdir)/bin
install_etc=$(installdir)/etc
install_lib=$(installdir)/lib
install_template=$(installdir)/template
install_man=$(installdir)/man
subdirs=src/net src/lib src/tsp src/xml platform/solaris template conf man
ifname=ip.tun0
LIBS=-lsocket -lnsl
