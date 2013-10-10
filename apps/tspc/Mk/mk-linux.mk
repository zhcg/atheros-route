#------------------------------------------------------------------------------
# mk-linux.mk,v 1.3 2004/02/12 23:05:08 dgregoire Exp
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
# Linux particulars.
#
DEFINES=-Dlinux
COPY=cp
TSPC=tspc
prefix=${INSTROOT}
install_bin=$(prefix)/bin
install_etc=$(prefix)/etc
install_lib=$(prefix)/lib
install_template=$(prefix)/template
install_man=$(prefix)/man
subdirs=src/net src/lib src/tsp src/xml platform/linux template conf man 
ifname=sit1
ifname_tun=tun
