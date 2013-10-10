@echo off
rem checktunnel.bat,v 1.4 2004/07/07 19:15:03 dgregoire Exp
rem
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

echo Host type: [%TSP_HOST_TYPE%]
echo.
echo Tunnel end-points:
echo Local interface: [%TSP_TUNNEL_INTERFACE%]
echo.
echo Client: v4 [%TSP_CLIENT_ADDRESS_IPV4%]
echo         v6 [%TSP_CLIENT_ADDRESS_IPV6%/%TSP_TUNNEL_PREFIXLEN%]
echo.
echo Server: v4 [%TSP_SERVER_ADDRESS_IPV4%]
echo         v6 [%TSP_SERVER_ADDRESS_IPV6%/%TSP_TUNNEL_PREFIXLEN%]
echo.
echo Tunnel mode: [%TSP_TUNNEL_MODE%]
echo.

if "%TSP_HOST_TYPE%" == "ROUTER" goto router
if "%TSP_HOST_TYPE%" == "router" goto router
goto end
:router
echo Routing information:
echo Home network interface: [%TSP_HOME_INTERFACE%]
echo   (for prefix advertisement)
echo.
echo Prefix: [%TSP_PREFIX%/%TSP_PREFIXLEN%]
echo.

:end
echo.
