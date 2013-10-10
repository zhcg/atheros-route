/* includes/config.h.  Generated from config.h.in by configure.  */
/* includes/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to compile debug-only DHCP software. */
#undef DEBUG

/* Define to BIG_ENDIAN for MSB (Motorola or SPARC CPUs) or LITTLE_ENDIAN for
   LSB (Intel CPUs). */
#define DHCP_BYTE_ORDER LITTLE_ENDIAN

/* Define to 1 to include DHCPv6 support. */
#undef DHCPv6 

/* Define to any value to chroot() prior to loading config. */
#undef EARLY_CHROOT 

/* Define to include execute() config language support. */
#undef ENABLE_EXECUTE 

/* Define to include Failover Protocol support. */
#undef FAILOVER_PROTOCOL 

/* Define to 1 if you have the /dev/random file. */
#define HAVE_DEV_RANDOM 1

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <linux/types.h> header file. */
#define HAVE_LINUX_TYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <net/if6.h> header file. */
/* #undef HAVE_NET_IF6_H */

/* Define to 1 if you have the <net/if_dl.h> header file. */
/* #undef HAVE_NET_IF_DL_H */

/* Define to 1 if you have the <regex.h> header file. */
#define HAVE_REGEX_H 1

/* Define to 1 if the sockaddr structure has a length field. */
/* #undef HAVE_SA_LEN */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has 'struct if_laddrconf'. */
/* #undef ISC_PLATFORM_HAVEIF_LADDRCONF */

/* Define to 1 if the system has 'struct if_laddrreq'. */
/* #undef ISC_PLATFORM_HAVEIF_LADDRREQ */

/* Define to 1 if the system has 'struct lifnum'. */
/* #undef ISC_PLATFORM_HAVELIFNUM */

/* Define to 1 if the inet_aton() function is missing. */
/* #undef NEED_INET_ATON */

/* Name of package */
#define PACKAGE "dhcp"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "dhcp-users@isc.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "DHCP"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "DHCP 4.1.0a2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "dhcp"

/* Define to the version of this package. */
#define PACKAGE_VERSION "4.1.0a2"

/* Define to any value to include Ari's PARANOIA patch. */
#undef PARANOIA 

/* The size of `struct iaddr *', as computed by sizeof. */
#define SIZEOF_STRUCT_IADDR_P 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to include server activity tracing support. */
#undef TRACING 

/* Define to 1 to use the Berkeley Packet Filter interface code. */
#undef USE_BPF 

/* Define to 1 to use DLPI interface code. */
#undef USE_DLPI 

/* Define to 1 to use the Linux Packet Filter interface code. */
#define USE_LPF 1

/* Version number of package */
#define VERSION "4.1.0a2"

/* File for dhclient6 leases. */
#undef _PATH_DHCLIENT6_DB

/* File for dhclient6 process information. */
#undef _PATH_DHCLIENT6_PID 

/* File for dhclient leases. */
/* #undef _PATH_DHCLIENT_DB */

/* File for dhclient process information. */
/* #undef _PATH_DHCLIENT_PID */

/* File for dhcpd6 leases. */
/* #undef _PATH_DHCPD6_DB */

/* File for dhcpd6 process information. */
/* #undef _PATH_DHCPD6_PID */

/* File for dhcpd leases. */
/* #undef _PATH_DHCPD_DB */

/* File for dhcpd process information. */
/* #undef _PATH_DHCPD_PID */

/* File for dhcrelay process information. */
/* #undef _PATH_DHCRELAY_PID */
