/*
 * define path names
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/pppoe/ppp-2.4.4/pppd/pathnames.h#1 $
 */

#ifdef HAVE_PATHS_H
#include <paths.h>

#else /* HAVE_PATHS_H */
#ifndef _PATH_VARRUN
#define _PATH_VARRUN 	"/var/local/tmp/ppp/"
#endif
#define _PATH_DEVNULL	"/dev/null"
#endif /* HAVE_PATHS_H */

#ifndef _ROOT_PATH
#define _ROOT_PATH
#endif
/*Ming*/
#define _PATH_PPP_PB44_PREFIX  "/var/local/tmp/ppp/"
#define _PATH_PPP_STATUS _PATH_PPP_PB44_PREFIX  "status/" 

#define _PATH_UPAPFILE 	 _ROOT_PATH "/var/local/tmp/ppp/pap-secrets"
#define _PATH_CHAPFILE 	 _ROOT_PATH "/var/local/tmp/ppp/chap-secrets"
#define _PATH_SRPFILE 	 _ROOT_PATH "/var/local/tmp/ppp/srp-secrets"
#define _PATH_SYSOPTIONS _ROOT_PATH "/var/local/tmp/ppp/options"
#define _PATH_IPUP	 _ROOT_PATH "/var/local/tmp/ppp/ip-up1"
#define _PATH_IPDOWN	 _ROOT_PATH "/etc/ppp/ip-down"
#define _PATH_IPPREUP	 _ROOT_PATH "/etc/ppp/ip-pre-up"
#define _PATH_AUTHUP	 _ROOT_PATH "/var/local/tmp/ppp/auth-up"
#define _PATH_AUTHDOWN	 _ROOT_PATH "/var/local/tmp/ppp/auth-down"
#define _PATH_TTYOPT	 _ROOT_PATH "/var/local/tmp/ppp/options.1"
#define _PATH_CONNERRS	 _ROOT_PATH "/var/local/tmp/ppp/connect-errors1"
#define _PATH_PEERFILES	 _ROOT_PATH "/var/local/tmp/ppp/peers/1"
//#define _PATH_RESOLV	 _ROOT_PATH "/etc/ppp/resolv.conf"
#define _PATH_RESOLV	 _ROOT_PATH "/var/local/tmp/resolv.conf"
#define _PATH_USEROPT	 ".ppprc"
#define	_PATH_PSEUDONYM	 ".ppp_pseudonym"

#ifdef INET6
#define _PATH_IPV6UP     _ROOT_PATH "/var/local/tmp/ppp/ipv6-up"
#define _PATH_IPV6DOWN   _ROOT_PATH "/var/local/tmp/ppp/ipv6-down"
#endif

#ifdef IPX_CHANGE
#define _PATH_IPXUP	 _ROOT_PATH "/var/local/tmp/ppp/ipx-up"
#define _PATH_IPXDOWN	 _ROOT_PATH "/var/local/tmp/ppp/ipx-down"
#endif /* IPX_CHANGE */

#ifdef __STDC__
#define _PATH_PPPDB	"/var/local/tmp/ppp/pppd2.tdb"
#else /* __STDC__ */
#ifdef HAVE_PATHS_H
#define _PATH_PPPDB	"/var/local/tmp/ppp/pppd2.tdb"
#else
#define _PATH_PPPDB	"/var/local/tmp/ppp/pppd2.tdb"
#endif
#endif /* __STDC__ */

#ifdef PLUGIN
#ifdef __STDC__
#define _PATH_PLUGIN	DESTDIR "/lib/pppd/" VERSION
#else /* __STDC__ */
#define _PATH_PLUGIN	"/usr/lib/pppd"
#endif /* __STDC__ */

#endif /* PLUGIN */
