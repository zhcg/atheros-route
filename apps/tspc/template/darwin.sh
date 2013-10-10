#!/bin/sh -x
#darwin.sh,v 1.5 2004/07/07 19:15:03 dgregoire Exp
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

LANGUAGE=C

if [ -z $TSP_VERBOSE ]; then
   TSP_VERBOSE=0
fi

KillProcess()
{
   if [ ! -z $TSP_VERBOSE ]; then
      if [ $TSP_VERBOSE -ge 2 ]; then
         echo killing $*
      fi
   fi
   PIDs=`ps axww | grep $1 | grep -v grep | awk '{ print $1;}'`
   echo $PIDs
   for PID in $PIDs
   do
      if [ ! -z $PID ]; then
         kill $PID
      fi
   done
}

Display()
{
   if [ -z $TSP_VERBOSE ]; then
      return;
   fi
   if [ $TSP_VERBOSE -lt $1 ]; then
      return;
   fi
   shift
   echo $*
}

Exec()
{
   if [ ! -z $TSP_VERBOSE ]; then
      if [ $TSP_VERBOSE -ge 2 ]; then
         echo $*
      fi
   fi
   $* # Execute command
   if [ $? -ne 0 ]; then
      echo "Error while executing $1"
      echo "   Command: $*"
      exit 1
   fi
}

ExecNoCheck()
{
   if [ ! -z $TSP_VERBOSE ]; then
      if [ $TSP_VERBOSE -ge 2 ]; then
         echo $*
      fi
   fi
   $* # Execute command
}

# Programs localization 

Display 1 "--- Start of configuration script. ---"
Display 1 "Script: " `basename $0`

ifconfig=/sbin/ifconfig
route=/sbin/route
rtadvd=/usr/sbin/rtadvd
sysctl=/usr/sbin/sysctl

rtadvdconfigfile=$TSP_HOME_DIR/rtadvd.conf

if [ -z $TSP_HOME_DIR ]; then
   echo "TSP_HOME_DIR variable not specified!;"
   exit 1
fi

if [ ! -d $TSP_HOME_DIR ]; then
   echo "Error : directory $TSP_HOME_DIR does not exist"
   exit 1
fi
#

if [ -z $TSP_HOST_TYPE ]; then
   echo Error: TSP_HOST_TYPE not defined.
   exit 1
fi

if [ X"${TSP_HOST_TYPE}" = X"host" ] || [ X"${TSP_HOST_TYPE}" = X"router" ]; then
   #
   # Configured tunnel config (IPv4)
   Display 1 Setting up interface $TSP_TUNNEL_INTERFACE
# Delete first the any previous tunnel

   ExecNoCheck $ifconfig $TSP_TUNNEL_INTERFACE deletetunnel
   Exec $ifconfig $TSP_TUNNEL_INTERFACE tunnel $TSP_CLIENT_ADDRESS_IPV4 $TSP_SERVER_ADDRESS_IPV4

   #
   # Configured tunnel config (IPv6) 

   # Check if the interface already has an IPv6 configuration
   list=`$ifconfig $TSP_TUNNEL_INTERFACE | grep inet6 | awk '{print $2}' | grep -v '^fe80'`
   for ipv6address in $list
   do 
      Exec $ifconfig $TSP_TUNNEL_INTERFACE inet6 $ipv6address delete
   done

   Exec $ifconfig $TSP_TUNNEL_INTERFACE inet6 $TSP_CLIENT_ADDRESS_IPV6 $TSP_SERVER_ADDRESS_IPV6 prefixlen $TSP_TUNNEL_PREFIXLEN alias

   # 
   # Default route  
   Display 1 Adding default route to $TSP_SERVER_ADDRESS_IPV6

   # Delete first any default IPv6 route
   ExecNoCheck $route delete -inet6 default
   Exec $route add -inet6 default $TSP_SERVER_ADDRESS_IPV6
fi

# Router configuration if required
if [ X"${TSP_HOST_TYPE}" = X"router" ]; then
   Display 1 "Router configuration"
   Display 1 "Kernel setup"

   Display 1 "Adding prefix to $TSP_HOME_INTERFACE"
   Exec $ifconfig $TSP_HOME_INTERFACE inet6 $TSP_PREFIX::1 prefixlen 64
   Exec $sysctl -w net.inet6.ip6.forwarding=1     # ipv6_forwarding enabled
   Exec $sysctl -w net.inet6.ip6.accept_rtadv=0   # routed must disable any router advertisement incoming
   if [ X"${TSP_PREFIXLEN}" != X"64" ]; then
      ExecNoCheck $route add -inet6 $TSP_PREFIX::  -prefixlen $TSP_PREFIXLEN ::1
   fi
   # Router advertisement configuration 
   Display 1 Create new $rtadvdconfigfile
   echo "##### rtadvd.conf made by TSP ####" > "$rtadvdconfigfile"
   echo "default:\\ " >> "$rtadvdconfigfile"
   echo "       :raflags#0:rltime#3600:\\" >> "$rtadvdconfigfile"
   echo "       :pinfoflags#64:vltime#360000:pltime#360000:mtu#1500:" >> "$rtadvdconfigfile"
   echo "ether:\\" >> "$rtadvdconfigfile"
   echo "       :mtu#1280:tc=default:" >> "$rtadvdconfigfile"
   echo "# interfaces." >> "$rtadvdconfigfile"
   echo "$TSP_HOME_INTERFACE:\\" >> "$rtadvdconfigfile"
   echo "       :addrs#1:\\" >> "$rtadvdconfigfile"
   echo '       :addr="'$TSP_PREFIX'::":prefixlen#64:tc=ether:' >> "$rtadvdconfigfile" 

   if [ -f $rtadvdconfigfile ]; then
      KillProcess $rtadvdconfigfile
      Exec $rtadvd -c $rtadvdconfigfile $TSP_HOME_INTERFACE  
   else
      echo "Error : file $rtadvdconfigfile not found"
      exit 1
   fi
fi

Display 1 "--- End of configuration script. ---"

exit 0
