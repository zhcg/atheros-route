#------------------------------------------------------------------------------
# mk-windows.mk,v 1.7 2004/07/14 18:08:03 lefebvre Exp
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
# Windows particulars.
#
DEFINES=-DWIN32
COPY=cp
TSPC=tspc.exe
installdir=platform/windows/nsis-installer-code
install_bin=$(installdir)
install_etc=$(installdir)/etc
install_lib=$(installdir)/lib
install_template=$(installdir)/template
subdirs=src/net src/lib src/xml src/tsp platform/windows conf template
ifname=2
ifname_tun=tunv6
template=windows.bat
