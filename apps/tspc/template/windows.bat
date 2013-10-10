@echo off
Rem #windows.bat,v 1.35 2004/09/21 12:54:03 dgregoire Exp
Rem
rem This source code copyright (c) Hexago Inc. 2002-2004.
rem
rem This program is free software; you can redistribute it and/or modify it 
rem under the terms of the GNU General Public License (GPL) Version 2, 
rem June 1991 as published by the Free  Software Foundation.
rem
rem This program is distributed in the hope that it will be useful, 
rem but WITHOUT ANY WARRANTY;  without even the implied warranty of 
rem MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
rem See the GNU General Public License for more details.
rem
rem You should have received a copy of the GNU General Public License 
rem along with this program; see the file GPL_LICENSE.txt. If not, write 
rem to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
rem MA 02111-1307 USA
rem
Rem 
Rem **************************************
Rem * Tunnel Server Protocol version 1.0 *
Rem * Host configuration script          *
Rem * Windows XP is supported            *
Rem **************************************
Rem * This script keeps a log

set LOG=NUL
set LOG2=NUL
if     %TSP_VERBOSE% == 3 set LOG2=CON
if not %TSP_VERBOSE% == 0 set LOG=CON

date /T 
time /T 

set NEWSTACK=NO
set WINDOWS_VERSION=0

Rem ***** Script an IPv6 tunnel "on-line" **********

:begin
echo Testing IPv6 presence
ping6 -n 1 ::1 > NUL
if errorlevel 1 goto fail0

echo Testing Windows NT version

Rem OS might have been preseted
if X%TSP_CLIENT_OS% == X goto testver

Echo Got the OS value %TSP_CLIENT_OS% preset from the broker
if %TSP_CLIENT_OS% == winXPSP1 set WINDOWS_VERSION=1
if %TSP_CLIENT_OS% == winXPSP2 set WINDOWS_VERSION=2
if %TSP_CLIENT_OS% == winXP set WINDOWS_VERSION=3
if %TSP_CLIENT_OS% == win2000 set WINDOWS_VERSION=4

if not %WINDOWS_VERSION% == 0 goto donttestver

:testver
Rem Use internal tool to determine version
"%TSP_HOME_DIR%\template\win-ver.exe"
set WINDOWS_VERSION=%errorlevel%

:donttestver

if %WINDOWS_VERSION% == 1 goto windows_xp
if %WINDOWS_VERSION% == 2 goto windows_xp
if %WINDOWS_VERSION% == 3 goto windows_xp
if %WINDOWS_VERSION% == 4 goto windows_2000

Rem *** Windows XP script path ***

:windows_xp
echo IPv4 tunnel server address configured : %TSP_SERVER_ADDRESS_IPV4% 
echo Resetting all the interfaces and routes
netsh.exe interface ipv6 reset

Rem are we setting up nat traversal or V6V4?
if %TSP_TUNNEL_MODE% == v6udpv4 goto nt_config

Rem can we spawn our own GIF interface on this os?
if %WINDOWS_VERSION% == 1 goto create_gif
if %WINDOWS_VERSION% == 2 goto create_gif
goto older_xp

:create_gif
echo Configuring V6V4 for XP Service Pack 1 and newer
echo Overriding TSP_TUNNEL_INTERFACE from %TSP_TUNNEL_INTERFACE% to hexago_tunv6
set TSP_TUNNEL_INTERFACE=hexago_tunv6

Rem set what we change to be non persistant
set STORAGEARG=store=active

netsh.exe interface ipv6 add v6v4tunnel %TSP_TUNNEL_INTERFACE% %TSP_CLIENT_ADDRESS_IPV4% %TSP_SERVER_ADDRESS_IPV4%
if errorlevel 1 goto fail9

netsh.exe interface ipv6 add address %TSP_TUNNEL_INTERFACE% %TSP_CLIENT_ADDRESS_IPV6% %STORAGEARG%
if errorlevel 1 goto fail2

netsh.exe interface ipv6 add route ::/0 %TSP_TUNNEL_INTERFACE% publish=yes %STORAGEARG%
if errorlevel 1 goto fail1

goto set_mtu

:older_xp
echo Configuring V6V4 for XP with no service pack
netsh.exe interface ipv6 add route prefix=::/0 interface=2 nexthop=::%TSP_SERVER_ADDRESS_IPV4% publish=yes %STORAGEARG% 
if errorlevel 1 goto fail1
           
netsh.exe interface ipv6 add address interface=2 address=%TSP_CLIENT_ADDRESS_IPV6% %STORAGEARG% 
if errorlevel 1 goto fail2

:set_mtu
Rem We skip mtu setting if in windows XP without any SP, it doesnt support it
if %WINDOWS_VERSION% == 3 goto check_router_config
echo Setting MTU to 1280 on tunnel interface "%TSP_TUNNEL_INTERFACE%"
netsh.exe interface ipv6 set interface interface="%TSP_TUNNEL_INTERFACE%" mtu=1280 
if errorlevel 1 goto fail8

:check_router_config
if %TSP_HOST_TYPE% == router GOTO router_config
goto success

:nt_config
echo Configuring for V6UDPV4 (NAT traversal)

netsh.exe interface ipv6 add address "%TSP_TUNNEL_INTERFACE%" %TSP_CLIENT_ADDRESS_IPV6% %STORAGEARG%
if errorlevel 1 goto fail3

Rem We skip mtu setting if in windows XP without any SP, it doesnt support it
if %WINDOWS_VERSION% == 3 goto skip_set_mtu
echo Setting MTU to 1280 on tunnel interface "%TSP_TUNNEL_INTERFACE%"
netsh.exe interface ipv6 set interface interface="%TSP_TUNNEL_INTERFACE%" mtu=1280 
if errorlevel 1 goto fail8

:skip_set_mtu
netsh.exe interface ipv6 add route %TSP_SERVER_ADDRESS_IPV6%/128 "%TSP_TUNNEL_INTERFACE%" %STORAGEARG% 
if errorlevel 1 goto fail3

netsh.exe interface ipv6 add route ::/0 "%TSP_TUNNEL_INTERFACE%" %TSP_SERVER_ADDRESS_IPV6% publish=yes %STORAGEARG% 
if errorlevel 1 goto fail3

if %TSP_HOST_TYPE% == router GOTO router_config
goto success

:router_config
echo This computer will be configured as IPv6 router and it will do router advertisements for autoconfiguration 
echo Configuring the %TSP_HOME_INTERFACE%  
netsh.exe interface ipv6 add address "%TSP_HOME_INTERFACE%" %TSP_PREFIX%::1 %STORAGEARG%
if errorlevel 1 goto fail6

echo Configuring IPv6_forwading on every the internal and external interfaces 

netsh.exe interface ipv6 set interface interface="%TSP_TUNNEL_INTERFACE%" forwarding=enabled %STORAGEARG%
netsh.exe interface ipv6 set interface interface="%TSP_HOME_INTERFACE%" forwarding=enabled %STORAGEARG%
netsh.exe interface ipv6 set interface interface=2 forwarding=enabled %STORAGEARG% 

Rem fix for XP with no service pack. forwarding only works if enabled 
Rem on the 6to4 interface. 
if %WINDOWS_VERSION% == 3 netsh.exe interface ipv6 set interface interface=3 forwarding=enabled %STORAGEARG% 

echo Configuring routing advertisement on the specified interface 
netsh.exe interface ipv6 set interface "%TSP_HOME_INTERFACE%" advertise=enabled %STORAGEARG% 
if errorlevel 1 goto fail7

echo You got the IPv6 prefix : %TSP_PREFIX%/%TSP_PREFIXLEN% 
netsh.exe interface ipv6 add route prefix=%TSP_PREFIX%::/64 interface="%TSP_HOME_INTERFACE%" siteprefixlength=64 publish=yes %STORAGEARG% 
if errorlevel 1 goto fail4
goto success


Rem *** Windows 2000 script path ***

:windows_2000
if %TSP_TUNNEL_MODE% == v6udpv4 goto fail3
echo IPv4 tunnel server address configured : %TSP_SERVER_ADDRESS_IPV4%
ipv6.exe rtu ::/0 2/::%TSP_SERVER_ADDRESS_IPV4% pub
if errorlevel 1 goto fail1

echo IPv6 host address configured : %TSP_CLIENT_ADDRESS_IPV6% 
ipv6.exe adu 2/%TSP_CLIENT_ADDRESS_IPV6% 
if errorlevel 1 goto fail2
if %TSP_HOST_TYPE% == router GOTO router_config_2000
goto success

:router_config_2000
echo This computer will be configured as IPv6 router and it will do router advertisements for autoconfiguration > %LOG%
echo Configuring IPv6_forwarding

Rem we turn on forwarding on all interfaces here
Rem to address the fact we need to enable it
Rem on the 6to4 interface and its interface
Rem number is variable

ipv6.exe ifc %TSP_HOME_INTERFACE% forwards
ipv6.exe ifc 2 forwards  
ipv6.exe ifc 3 forwards 
ipv6.exe ifc 4 forwards 
ipv6.exe ifc 5 forwards 
ipv6.exe ifc 6 forwards 
ipv6.exe ifc 7 forwards 
ipv6.exe ifc 8 forwards

echo Configuring routing advertisement on the specified interface 
ipv6.exe ifc %TSP_HOME_INTERFACE% advertises

echo You got the IPv6 prefix : %TSP_PREFIX%::/%TSP_PREFIXLEN% 
echo Your network interface %TSP_HOME_INTERFACE% will advertise %TSP_PREFIX%::/64 
ipv6.exe rtu %TSP_PREFIX%::/64 %TSP_HOME_INTERFACE% publish life 86400 spl 64 
if errorlevel 1 goto fail4
echo Address %TSP_PREFIX%::1 confired on interface %TSP_HOME_INTERFACE%
ipv6.exe adu %TSP_HOME_INTERFACE%/%TSP_PREFIX%::1 
goto success

:fail0
echo Failed : IPv6 not installed. Use the command "ipv6 install" to enable it. 
exit 1
goto end

:fail1
Echo Failed : not possible to use IPv4 tunnel server address 
exit 1
goto end

:fail2
Echo Failed : not possible to use IPv6 host address 
exit 1
goto end

:fail3
Echo Failed: not possible to setup net traversal 
exit 1
goto end

:fail4
Echo Failed : not possible to assign the %TSP_PREFIX% route on interface %TSP_HOME_INTERFACE%
exit 1
goto end

:fail5 
Echo Failed : not possible to disable the firewall on the outgoing interface 
exit 1
goto end

:fail6 
Echo Failed : not possible to configure %TSP_PREFIX% on the %TSP_HOME_INTERFACE% interface 
exit 1
goto end

:fail7 
Echo Failed : not possible to advertise the prefix %TSP_PREFIX%/64 on interface %TSP_HOME_INTERFACE% 
exit 1
goto end

:fail8
Echo Failed : not possible to set the MTU to 1280 on the tunnel interface 
exit 1
goto end

:fail9
Echo Failed : not possible to create a new v6v4 tunnel interface
exit 1
goto end

:success
Echo Success ! Now, you're ready to use IPv6 connectivity to Internet IPv6
Echo Your host is configured to use this IPv6 address : %TSP_CLIENT_ADDRESS_IPV6%
if %TSP_HOST_TYPE% == router GOTO success2
goto end

:success2
Echo The tunnel server uses this IPv6 address : %TSP_SERVER_ADDRESS_IPV6%
Echo The prefix advertised is %TSP_PREFIX%/64 on interface %TSP_HOME_INTERFACE%
goto end

:end

Echo End of the script


