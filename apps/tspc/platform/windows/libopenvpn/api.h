/*
---------------------------------------------------------------------------
 api.h,v 1.4 2004/03/03 23:52:11 dgregoire Exp
---------------------------------------------------------------------------
Copyright (c) 2001-2003 Hexago Inc. All rights reserved.

     The contents of this file are subject to the Hexago
     Public License Version 1.0 (the "License"); you may not
     use this file except in compliance with the License.

     Software distributed under the License is distributed on
     an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
     express or implied. See the License for the specific
     language governing rights and limitations under the
     License.

     The Original Code is _source code of the tunnel server
     protocol-client side_.

     The Initial Developer of the Original Code is Hexago .

     All Rights Reserved.

     Contributor(s): ______________________________________.

---------------------------------------------------------------------------
*/

char *
TunGetTunName();

int
TunMainLoop(SOCKET s, char *tun_dev, char *local_ipv4, int local_port, char *remote_ipv4, int remote_port,
			int debug_level, int keepalive,
			int keepalive_interval, char *local_ipv6, char *remote_keepalive_ipv6);
