/*
 * arp                This file contains an implementation of the command
 *              that maintains the kernel's ARP cache.  It is derived
 *              from Berkeley UNIX arp(8), but cleaner and with sup-
 *              port for devices other than Ethernet.
 *
 * NET-TOOLS    A collection of programs that form the base set of the
 *              NET-3 Networking Distribution for the LINUX operating
 *              system.
 *
 * Version:     $Id: arp.c,v 1.20 2001/04/08 17:05:05 pb Exp $
 *
 * Maintainer:  Bernd 'eckes' Eckenfels, <net-tools@lina.inka.de>
 *
 * Author:      Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *
 * Changes:
 *              (based on work from Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>)
 *              Alan Cox        :       modified for NET3
 *              Andrew Tridgell :       proxy arp netmasks
 *              Bernd Eckenfels :       -n option
 *              Bernd Eckenfels :       Use only /proc for display
 *       {1.60} Bernd Eckenfels :       new arpcode (-i) for 1.3.42 but works 
 *                                      with 1.2.x, too
 *       {1.61} Bernd Eckenfels :       more verbose messages
 *       {1.62} Bernd Eckenfels :       check -t for hw adresses and try to
 *                                      explain EINVAL (jeff)
 *970125 {1.63} Bernd Eckenfels :       -a print hardwarename instead of tiltle
 *970201 {1.64} Bernd Eckenfels :       net-features.h support
 *970203 {1.65} Bernd Eckenfels :       "#define" in "#if", 
 *                                      -H|-A additional to -t|-p
 *970214 {1.66} Bernd Eckenfels :       Fix optarg required for -H and -A
 *970412 {1.67} Bernd Eckenfels :       device=""; is default
 *970514 {1.68} Bernd Eckenfels :       -N and -D
 *970517 {1.69} Bernd Eckenfels :       usage() fixed
 *970622 {1.70} Bernd Eckenfels :       arp -d priv
 *970106 {1.80} Bernd Eckenfels :       new syntax without -D and with "dev <If>",
 *                                      ATF_MAGIC, ATF_DONTPUB support. 
 *                                      Typo fix (Debian Bug#5728 Giuliano Procida)
 *970803 {1.81} Bernd Eckenfels :       removed junk comment line 1
 *970925 {1.82} Bernd Eckenfels :       include fix for libc6
 *980213 (1.83) Phil Blundell:          set ATF_COM on new entries
 *980629 (1.84) Arnaldo Carvalho de Melo: gettext instead of catgets
 *990101 {1.85} Bernd Eckenfels		fixed usage and return codes
 *990105 (1.86) Phil Blundell:		don't ignore EINVAL in arp_set
 *991121 (1.87) Bernd Eckenfels:	yes --device has a mandatory arg
 *010404 (1.88) Arnaldo Carvalho de Melo: use setlocale
 *
 *              This program is free software; you can redistribute it
 *              and/or  modify it under  the terms of  the GNU General
 *              Public  License as  published  by  the  Free  Software
 *              Foundation;  either  version 2 of the License, or  (at
 *              your option) any later version.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
/* #include <linux/netdevice.h> */
/* #include <linux/if_arp.h>    */
#include <net/if_arp.h>
#include <linux/if_ether.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>

#define DFLT_AF	"inet"
#define DFLT_HW	"ether"

#define FEATURE_ARP
#define _PATH_PROCNET_ARP		"/proc/net/arp"
#define FLAG_NUM_HOST  4
#define FLAG_NUM_PORT  8
#define FLAG_NUM_USER 16
#define FLAG_NUM     (FLAG_NUM_HOST|FLAG_NUM_PORT|FLAG_NUM_USER)
#define FLAG_SYM      32

#define ENOSUPP(A,B)	fprintf(stderr,\
                                "%s: feature `%s' not supported.\n" \
				  "Please recompile `net-tools' with "\
				  "newer kernel source or full configuration.\n",A,B)



int opt_n = 0;			/* do not resolve addresses     */
int opt_N = 0;			/* use symbolic names           */
int opt_v = 0;			/* debugging output flag        */
int opt_D = 0;			/* HW-address is devicename     */
int opt_e = 0;			/* 0=BSD output, 1=new linux    */
int opt_a = 0;			/* all entries, substring match */
struct aftype *ap;		/* current address family       */
struct hwtype *hw;		/* current hardware type        */
int sockfd = 0;			/* active socket descriptor     */
int hw_set = 0;			/* flag if hw-type was set (-H) */
char device[16] = "";		/* current device               */
static void usage(void);

static char *pr_ether(unsigned char *ptr);
static int in_ether(char *bufp, struct sockaddr *sap);
int INET_rresolve(char *name, size_t len, struct sockaddr_in *sin, 
			 int numeric, unsigned int netmask);


struct hwtype {
    char *name;
    char *title;
    int type;
    int alen;
    char *(*print) (unsigned char *);
    int (*input) (char *, struct sockaddr *);
    int (*activate) (int fd);
    int suppress_null_addr;
}ether_hwtype =
{
    .name	= "ether", 
	.title	= "Ethernet", /*"10Mbps Ethernet", */
	.type	= ARPHRD_ETHER,
	.alen	= ETH_ALEN,
    .print	= pr_ether, 
    .input	= in_ether,
};


/* Split the input string into multiple fields. */
int getargs(char *string, char *arguments[])
{
    int len = strlen(string); 
    char temp[len+1];
    char *sp, *ptr;
    int i, argc;
    char want;

    /*
     * Copy the string into a buffer.  We may have to modify
     * the original string because of all the quoting...
     */
    sp = string;
    i = 0;
    strcpy(temp, string);
    ptr = temp;

    /*
     * Look for delimiters ("); if present whatever
     * they enclose will be considered one argument.
     */
    while (*ptr != '\0' && i < 31) {
	/* Ignore leading whitespace on input string. */
	while (*ptr == ' ' || *ptr == '\t')
	    ptr++;

	/* Set string pointer. */
	arguments[i++] = sp;

	/* Check for any delimiters. */
	if (*ptr == '"' || *ptr == '\'') {
	    /*
	     * Copy the string up to any whitespace OR the next
	     * delimiter. If the delimiter was escaped, skip it
	     * as it if was not there.
	     */
	    want = *ptr++;
	    while (*ptr != '\0') {
		if (*ptr == want && *(ptr - 1) != '\\') {
		    ptr++;
		    break;
		}
		*sp++ = *ptr++;
	    }
	} else {
	    /* Just copy the string up to any whitespace. */
	    while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t')
		*sp++ = *ptr++;
	}
	*sp++ = '\0';

	/* Skip trailing whitespace. */
	if (*ptr != '\0') {
	    while (*ptr == ' ' || *ptr == '\t')
		ptr++;
	}
    }
    argc = i;
    while (i < 32)
	arguments[i++] = (char *) NULL;
    return (argc);
}

/* Like strncpy but make sure the resulting string is always 0 terminated. */  
char *safe_strncpy(char *dst, const char *src, size_t size)
{   
    dst[size-1] = '\0';
    return strncpy(dst,src,size-1);   
}

/* Display an Ethernet address in readable format. */
static char *pr_ether(unsigned char *ptr)
{
    static char buff[64];

    snprintf(buff, sizeof(buff), "%02X:%02X:%02X:%02X:%02X:%02X",
	     (ptr[0] & 0377), (ptr[1] & 0377), (ptr[2] & 0377),
	     (ptr[3] & 0377), (ptr[4] & 0377), (ptr[5] & 0377)
	);
    return (buff);
}


struct hwtype *get_hwntype(int type)
{
    if (type == ether_hwtype.type)
		return &ether_hwtype;
	else
		printf("Unknown hardware type\n");
	
    return (NULL);
}

struct hwtype *get_hwtype(const char *name)
{
	if (!strcmp(ether_hwtype.name, name))
	    return &ether_hwtype;
	else
		printf("Unknown hardware type\n");
	
    return (NULL);
}


static int INET_resolve(char *name, struct sockaddr_in *sin, int hostfirst)
{
    struct hostent *hp;
    struct netent *np;

    /* Grmpf. -FvK */
    sin->sin_family = AF_INET;
    sin->sin_port = 0;

    /* Default is special, meaning 0.0.0.0. */
    if (!strcmp(name, "default")) {
	sin->sin_addr.s_addr = INADDR_ANY;
	return (1);
    }
    /* Look to see if it's a dotted quad. */
    if (inet_aton(name, &sin->sin_addr)) {
	return 0;
    }
    /* If we expect this to be a hostname, try hostname database first */
#ifdef DEBUG
    if (hostfirst) fprintf (stderr, "gethostbyname (%s)\n", name);
#endif
    if (hostfirst && 
	(hp = gethostbyname(name)) != (struct hostent *) NULL) {
	memcpy((char *) &sin->sin_addr, (char *) hp->h_addr_list[0], 
		sizeof(struct in_addr));
	return 0;
    }
    /* Try the NETWORKS database to see if this is a known network. */
#ifdef DEBUG
    fprintf (stderr, "getnetbyname (%s)\n", name);
#endif
    if ((np = getnetbyname(name)) != (struct netent *) NULL) {
	sin->sin_addr.s_addr = htonl(np->n_net);
	return 1;
    }
    if (hostfirst) {
	/* Don't try again */
	errno = h_errno;
	return -1;
    }
#ifdef DEBUG
    res_init();
    _res.options |= RES_DEBUG;
#endif

#ifdef DEBUG
    fprintf (stderr, "gethostbyname (%s)\n", name);
#endif
    if ((hp = gethostbyname(name)) == (struct hostent *) NULL) {
	errno = h_errno;
	return -1;
    }
    memcpy((char *) &sin->sin_addr, (char *) hp->h_addr_list[0], 
	   sizeof(struct in_addr));

    return 0;
}

/* Input an Ethernet address and convert to binary. */
static int in_ether(char *bufp, struct sockaddr *sap)
{
    unsigned char *ptr;
    char c, *orig;
    int i;
    unsigned val;

    sap->sa_family = ARPHRD_ETHER;
    ptr = sap->sa_data;

    i = 0;
    orig = bufp;
    while ((*bufp != '\0') && (i < ETH_ALEN)) {
	val = 0;
	c = *bufp++;
	if (isdigit(c))
	    val = c - '0';
	else if (c >= 'a' && c <= 'f')
	    val = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    val = c - 'A' + 10;
	else {
#ifdef DEBUG
	    fprintf(stderr, ("in_ether(%s): invalid ether address!\n"), orig);
#endif
	    errno = EINVAL;
	    return (-1);
	}
	val <<= 4;
	c = *bufp;
	if (isdigit(c))
	    val |= c - '0';
	else if (c >= 'a' && c <= 'f')
	    val |= c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    val |= c - 'A' + 10;
	else if (c == ':' || c == 0)
	    val >>= 4;
	else {
#ifdef DEBUG
	    fprintf(stderr, ("in_ether(%s): invalid ether address!\n"), orig);
#endif
	    errno = EINVAL;
	    return (-1);
	}
	if (c != 0)
	    bufp++;
	*ptr++ = (unsigned char) (val & 0377);
	i++;

	/* We might get a semicolon here - not required. */
	if (*bufp == ':') {
	    if (i == ETH_ALEN) {
#ifdef DEBUG
		fprintf(stderr, ("in_ether(%s): trailing : ignored!\n"),
			orig)
#endif
		    ;		/* nothing */
	    }
	    bufp++;
	}
    }

    /* That's it.  Any trailing junk? */
    if ((i == ETH_ALEN) && (*bufp != '\0')) {
#ifdef DEBUG
	fprintf(stderr, ("in_ether(%s): trailing junk!\n"), orig);
	errno = EINVAL;
	return (-1);
#endif
    }
#ifdef DEBUG
    fprintf(stderr, "in_ether(%s): %s\n", orig, pr_ether(sap->sa_data));
#endif

    return (0);
}

static char *INET_sprint(struct sockaddr *sap, int numeric)
{
    static char buff[128];

    if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
	return safe_strncpy(buff, "[NONE SET]", sizeof(buff));

    if (INET_rresolve(buff, sizeof(buff), (struct sockaddr_in *) sap, 
		      numeric, 0xffffff00) != 0)
	return (NULL);

    return (buff);
}

struct addr {
    struct sockaddr_in addr;
    char *name;
    int host;
    struct addr *next;
}*INET_nn = NULL;	/* addr-to-name cache           */

/* numeric: & 0x8000: default instead of *, 
 *	    & 0x4000: host instead of net, 
 *	    & 0x0fff: don't resolve
 */
int INET_rresolve(char *name, size_t len, struct sockaddr_in *sin, 
			 int numeric, unsigned int netmask)
{
    struct hostent *ent;
    struct netent *np;
    struct addr *pn;
    unsigned long ad, host_ad;
    int host = 0;

    /* Grmpf. -FvK */
    if (sin->sin_family != AF_INET) {
#ifdef DEBUG
	fprintf(stderr, _("rresolve: unsupport address family %d !\n"), sin->sin_family);
#endif
	errno = EAFNOSUPPORT;
	return (-1);
    }
    ad = (unsigned long) sin->sin_addr.s_addr;
#ifdef DEBUG
    fprintf (stderr, "rresolve: %08lx, mask %08x, num %08x \n", ad, netmask, numeric);
#endif
    if (ad == INADDR_ANY) {
	if ((numeric & 0x0FFF) == 0) {
	    if (numeric & 0x8000)
		safe_strncpy(name, "default", len);
	    else
	        safe_strncpy(name, "*", len);
	    return (0);
	}
    }
    if (numeric & 0x0FFF) {
        safe_strncpy(name, inet_ntoa(sin->sin_addr), len);
	return (0);
    }

    if ((ad & (~netmask)) != 0 || (numeric & 0x4000))
	host = 1;
#if 0
    INET_nn = NULL;
#endif
    pn = INET_nn;
    while (pn != NULL) {
	if (pn->addr.sin_addr.s_addr == ad && pn->host == host) {
	    safe_strncpy(name, pn->name, len);
#ifdef DEBUG
	    fprintf (stderr, "rresolve: found %s %08lx in cache\n", (host? "host": "net"), ad);
#endif
	    return (0);
	}
	pn = pn->next;
    }

    host_ad = ntohl(ad);
    np = NULL;
    ent = NULL;
    if (host) {
#ifdef DEBUG
	fprintf (stderr, "gethostbyaddr (%08lx)\n", ad);
#endif
	ent = gethostbyaddr((char *) &ad, 4, AF_INET);
	if (ent != NULL)
	    safe_strncpy(name, ent->h_name, len);
    } else {
#ifdef DEBUG
	fprintf (stderr, "getnetbyaddr (%08lx)\n", host_ad);
#endif
	np = getnetbyaddr(host_ad, AF_INET);
	if (np != NULL)
	    safe_strncpy(name, np->n_name, len);
    }
    if ((ent == NULL) && (np == NULL))
	safe_strncpy(name, inet_ntoa(sin->sin_addr), len);
    pn = (struct addr *) malloc(sizeof(struct addr));
    pn->addr = *sin;
    pn->next = INET_nn;
    pn->host = host;
    pn->name = (char *) malloc(strlen(name) + 1);
    strcpy(pn->name, name);
    INET_nn = pn;

    return (0);
}



/* Delete an entry from the ARP cache. */
static int arp_del(char **args)
{
    char host[128];
    struct arpreq req;
    struct sockaddr sa;
    int flags = 0;
    int err;

    memset((char *) &req, 0, sizeof(req));

    /* Resolve the host name. */
    if (*args == NULL) {
		fprintf(stderr, ("arp: need host name\n"));
		return (-1);
    }
    safe_strncpy(host, *args, (sizeof host));
    if (INET_resolve(host, (struct sockaddr_in*)&sa, 0) < 0) {
		printf("%s %s %s", __FILE__, __LINE__, host);
		return (-1);
    }
    /* If a host has more than one address, use the correct one! */
    memcpy((char *) &req.arp_pa, (char *) &sa, sizeof(struct sockaddr));

    if (hw_set)
	req.arp_ha.sa_family = ARPHRD_ETHER;

    req.arp_flags = ATF_PERM;
    args++;
    while (*args != NULL) {
	if (opt_v)
	    fprintf(stderr, "args=%s\n", *args);
	if (!strcmp(*args, "pub")) {
	    flags |= 1;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "priv")) {
	    flags |= 2;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "temp")) {
	    req.arp_flags &= ~ATF_PERM;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "trail")) {
	    req.arp_flags |= ATF_USETRAILERS;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "dontpub")) {
#ifdef HAVE_ATF_DONTPUB
	    req.arp_flags |= ATF_DONTPUB;
#else
	    ENOSUPP("arp", "ATF_DONTPUB");
#endif
	    args++;
	    continue;
	}
	if (!strcmp(*args, "auto")) {
#ifdef HAVE_ATF_MAGIC
	    req.arp_flags |= ATF_MAGIC;
#else
	    ENOSUPP("arp", "ATF_MAGIC");
#endif
	    args++;
	    continue;
	}
	if (!strcmp(*args, "dev")) {
	    if (*++args == NULL)
		usage();
	    safe_strncpy(device, *args, sizeof(device));
	    args++;
	    continue;
	}
	if (!strcmp(*args, "netmask")) {
	    if (*++args == NULL)
		usage();
	    if (strcmp(*args, "255.255.255.255") != 0) {
		strcpy(host, *args);
		if (INET_resolve(host, (struct sockaddr_in*)&sa, 0) < 0) {
		    printf("%s %s %s", __FILE__, __LINE__, host);
		    return (-1);
		}
		memcpy((char *) &req.arp_netmask, (char *) &sa,
		       sizeof(struct sockaddr));
		req.arp_flags |= ATF_NETMASK;
	    }
	    args++;
	    continue;
	}
	usage();
    }
    if (flags == 0)
	flags = 3;

    strcpy(req.arp_dev, device);

    err = -1;

    /* Call the kernel. */
    if (flags & 2) {
	if (opt_v)
	    fprintf(stderr, "arp: SIOCDARP(nopub)\n");
	if ((err = ioctl(sockfd, SIOCDARP, &req) < 0)) {
	    if (errno == ENXIO) {
		if (flags & 1)
		    goto nopub;
		printf(("No ARP entry for %s\n"), host);
		return (-1);
	    }
	    perror("SIOCDARP(priv)");
	    return (-1);
	}
    }
    if ((flags & 1) && (err)) {
      nopub:
	req.arp_flags |= ATF_PUBL;
	if (opt_v)
	    fprintf(stderr, "arp: SIOCDARP(pub)\n");
	if (ioctl(sockfd, SIOCDARP, &req) < 0) {
	    if (errno == ENXIO) {
		printf(("No ARP entry for %s\n"), host);
		return (-1);
	    }
	    perror("SIOCDARP(pub)");
	    return (-1);
	}
    }
    return (0);
}

/* Get the hardware address to a specified interface name */
static int arp_getdevhw(char *ifname, struct sockaddr *sa, struct hwtype *hw)
{
    struct ifreq ifr;
    struct hwtype *xhw;

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
		fprintf(stderr, ("arp: cant get HW-Address for `%s': %s.\n"), ifname, strerror(errno));
		return (-1);
    }
    if (hw && (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)) {
		fprintf(stderr, ("arp: protocol type mismatch.\n"));
		return (-1);
    }
    memcpy((char *) sa, (char *) &(ifr.ifr_hwaddr), sizeof(struct sockaddr));

#if 0
    if (opt_v) {
	if (!(xhw = get_hwntype(ifr.ifr_hwaddr.sa_family)) || (xhw->print == 0)) {
	    xhw = get_hwntype(-1);
	}
	fprintf(stderr, ("arp: device `%s' has HW address %s `%s'.\n"), ifname, xhw->name, xhw->print((char *)&ifr.ifr_hwaddr.sa_data));
    }
#endif

    return (0);
}

/* Set an entry in the ARP cache. */
static int arp_set(char **args)
{
    char host[128];
    struct arpreq req;
    struct sockaddr sa;
    int flags;

    memset((char *) &req, 0, sizeof(req));

    /* Resolve the host name. */
    if (*args == NULL) {
	fprintf(stderr, ("arp: need host name\n"));
	return (-1);
    }
    safe_strncpy(host, *args++, (sizeof host));
    if (INET_resolve(host, (struct sockaddr_in*)&sa, 0) < 0) {
	printf("%s %s %s", __FILE__, __LINE__, host);
	return (-1);
    }
    /* If a host has more than one address, use the correct one! */
    memcpy((char *) &req.arp_pa, (char *) &sa, sizeof(struct sockaddr));

    /* Fetch the hardware address. */
    if (*args == NULL) {
	fprintf(stderr, ("arp: need hardware address\n"));
	return (-1);
    }
    if (opt_D) {
	if (arp_getdevhw(*args++, &req.arp_ha, hw_set ? hw : NULL) < 0)
	    return (-1);
    } else {
	if (in_ether(*args++, &req.arp_ha) < 0) {
	    fprintf(stderr, ("arp: invalid hardware address\n"));
	    return (-1);
	}
    }

    /* Check out any modifiers. */
    flags = ATF_PERM | ATF_COM;
    while (*args != NULL) {
	if (!strcmp(*args, "temp")) {
	    flags &= ~ATF_PERM;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "pub")) {
	    flags |= ATF_PUBL;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "priv")) {
	    flags &= ~ATF_PUBL;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "trail")) {
	    flags |= ATF_USETRAILERS;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "dontpub")) {
#ifdef HAVE_ATF_DONTPUB
	    flags |= ATF_DONTPUB;
#else
	    ENOSUPP("arp", "ATF_DONTPUB");
#endif
	    args++;
	    continue;
	}
	if (!strcmp(*args, "auto")) {
#ifdef HAVE_ATF_MAGIC
	    flags |= ATF_MAGIC;
#else
	    ENOSUPP("arp", "ATF_MAGIC");
#endif
	    args++;
	    continue;
	}
	if (!strcmp(*args, "dev")) {
	    if (*++args == NULL)
		usage();
	    safe_strncpy(device, *args, sizeof(device));
	    args++;
	    continue;
	}
	if (!strcmp(*args, "netmask")) {
	    if (*++args == NULL)
		usage();
	    if (strcmp(*args, "255.255.255.255") != 0) {
		strcpy(host, *args);
		if (INET_resolve(host, (struct sockaddr_in*)&sa, 0) < 0) {
		    printf("%s %s %s", __FILE__, __LINE__, host);
		    return (-1);
		}
		memcpy((char *) &req.arp_netmask, (char *) &sa,
		       sizeof(struct sockaddr));
		flags |= ATF_NETMASK;
	    }
	    args++;
	    continue;
	}
	usage();
    }

    /* Fill in the remainder of the request. */
    req.arp_flags = flags;

    strcpy(req.arp_dev, device);

    /* Call the kernel. */
    if (opt_v)
	fprintf(stderr, "arp: SIOCSARP()\n");
    if (ioctl(sockfd, SIOCSARP, &req) < 0) {
        perror("SIOCSARP");
	return (-1);
    }
    return (0);
}


/* Process an EtherFile */
static int arp_file(char *name)
{
    char buff[1024];
    char *sp, *args[32];
    int linenr, argc;
    FILE *fp;

    if ((fp = fopen(name, "r")) == NULL) {
	fprintf(stderr, ("arp: cannot open etherfile %s !\n"), name);
	return (-1);
    }
    /* Read the lines in the file. */
    linenr = 0;
    while (fgets(buff, sizeof(buff), fp) != (char *) NULL) {
	linenr++;
	if (opt_v == 1)
	    fprintf(stderr, ">> %s", buff);
	if ((sp = strchr(buff, '\n')) != (char *) NULL)
	    *sp = '\0';
	if (buff[0] == '#' || buff[0] == '\0')
	    continue;

	argc = getargs(buff, args);
	if (argc < 2) {
	    fprintf(stderr, ("arp: format error on line %u of etherfile %s !\n"),
		    linenr, name);
	    continue;
	}
	if (strchr (args[0], ':') != NULL) {
	    /* We have a correct ethers file, switch hw adress and hostname
	       for arp */
	    char *cp;
	    cp = args[1];
	    args[1] = args[0];
	    args[0] = cp;
	}
	if (arp_set(args) != 0)
	    fprintf(stderr, ("arp: cannot set entry on line %u of etherfile %s !\n"),
		    linenr, name);
    }

    (void) fclose(fp);
    return (0);
}


/* Print the contents of an ARP request block. */
static void arp_disp_2(char *name, int type, int arp_flags, char *hwa, char *mask, char *dev)
{
    static int title = 0;
    struct hwtype *xhw;
    char flags[10];

    xhw = get_hwntype(type);
    if (xhw == NULL)
	xhw = get_hwtype(DFLT_HW);

    if (title++ == 0) {
	printf(("Address                  HWtype  HWaddress           Flags Mask            Iface\n"));
    }
    /* Setup the flags. */
    flags[0] = '\0';
    if (arp_flags & ATF_COM)
	strcat(flags, "C");
    if (arp_flags & ATF_PERM)
	strcat(flags, "M");
    if (arp_flags & ATF_PUBL)
	strcat(flags, "P");
#ifdef HAVE_ATF_MAGIC
    if (arp_flags & ATF_MAGIC)
	strcat(flags, "A");
#endif
#ifdef HAVE_ATF_DONTPUB
    if (arp_flags & ATF_DONTPUB)
	strcat(flags, "!");
#endif
    if (arp_flags & ATF_USETRAILERS)
	strcat(flags, "T");

    if (!(arp_flags & ATF_NETMASK))
	mask = "";

    printf("%-23.23s  ", name);

    if (!(arp_flags & ATF_COM)) {
	if (arp_flags & ATF_PUBL)
	    printf("%-8.8s%-20.20s", "*", "*");
	else
	    printf("%-8.8s%-20.20s", "", ("(incomplete)"));
    } else {
		printf("%-8.8s%-20.20s", xhw->name,
			hwa);
    }

    printf("%-6.6s%-15.15s %s\n", flags, mask, dev);
}

/* Print the contents of an ARP request block. */
static void arp_disp(char *name, char *ip, int type, int arp_flags, char *hwa, char *mask, char *dev)
{
    struct hwtype *xhw;

    xhw = get_hwntype(type);
    if (xhw == NULL)
	xhw = get_hwtype(DFLT_HW);

    printf(("%s (%s) at "), name, ip);

    if (!(arp_flags & ATF_COM)) {
	if (arp_flags & ATF_PUBL)
	    printf("* ");
	else
	    printf(("<incomplete> "));
    } else {
	printf("%s [%s] ", hwa, xhw->name);
    }

    if (arp_flags & ATF_NETMASK)
	printf(("netmask %s "), mask);

    if (arp_flags & ATF_PERM)
	printf("PERM ");
    if (arp_flags & ATF_PUBL)
	printf("PUP ");
#ifdef HAVE_ATF_MAGIC
    if (arp_flags & ATF_MAGIC)
	printf("AUTO ");
#endif
#ifdef HAVE_ATF_DONTPUB
    if (arp_flags & ATF_DONTPUB)
	printf("DONTPUB ");
#endif
    if (arp_flags & ATF_USETRAILERS)
	printf("TRAIL ");

    printf(("on %s\n"), dev);
	  
}


/* Display the contents of the ARP cache in the kernel. */
static int arp_show(char *name)
{
    char host[100];
    struct sockaddr sa;
    char ip[100];
    char hwa[100];
    char mask[100];
    char line[200];
    char dev[100];
    int type, flags;
    FILE *fp;
    char *hostname;
    int num, entries = 0, showed = 0;
	
	//luodp for webui
	FILE *fp2;
	char outBuff[128]={'\0'};
    if((fp2=fopen("/tmp/arplist","w+"))==NULL)
    {
		return ;
    }
	unsigned int count=1;
	//end luodp for webui
	
    host[0] = '\0';

    if (name != NULL) {
	/* Resolve the host name. */
	safe_strncpy(host, name, (sizeof host));
	if (INET_resolve(host, (struct sockaddr_in*)&sa, 0) < 0) {
	    printf("%s %s %s", __FILE__, __LINE__, host);
	    return (-1);
	}
	safe_strncpy(host, INET_sprint(&sa, 1), sizeof(host));
    }
    /* Open the PROCps kernel table. */
    if ((fp = fopen(_PATH_PROCNET_ARP, "r")) == NULL) {
	perror(_PATH_PROCNET_ARP);
	return (-1);
    }
    /* Bypass header -- read until newline */
    if (fgets(line, sizeof(line), fp) != (char *) NULL) {
	strcpy(mask, "-");
	strcpy(dev, "-");
	/* Read the ARP cache entries. */
	for (; fgets(line, sizeof(line), fp);) {
	    num = sscanf(line, "%s 0x%x 0x%x %100s %100s %100s\n",
			 ip, &type, &flags, hwa, mask, dev);
	    if (num < 4)
		break;

	    entries++;
	    /* if the user specified hw-type differs, skip it */
	    if (hw_set && (type != ARPHRD_ETHER))
		continue;

	    /* if the user specified address differs, skip it */
	    if (host[0] && strcmp(ip, host))
		continue;

	    /* if the user specified device differs, skip it */
	    if (device[0] && strcmp(dev, device))
		continue;

	    showed++;
	    /* This IS ugly but it works -be */
	    if (opt_n)
		hostname = "?";
	    else {
		if (INET_resolve(ip, (struct sockaddr_in*)&sa, 0) < 0)
		    hostname = ip;
		else
		    hostname = INET_sprint(&sa, opt_n | 0x8000);
		if (strcmp(hostname, ip) == 0)
		    hostname = "?";
	    }

	    if (opt_e)
		arp_disp_2(hostname[0] == '?' ? ip : hostname, type, flags, hwa, mask, dev);
	    else
		arp_disp(hostname, ip, type, flags, hwa, mask, dev);
		//luodp for webui
		if (flags & ATF_COM)
		{
			if (flags & ATF_PERM)
			{
				sprintf(outBuff,"<tr><td>%d</td><td>%s</td><td>%s</td><td>YES</td></tr>",count,hwa,ip);
			}else
			{
				sprintf(outBuff,"<tr><td>%d</td><td>%s</td><td>%s</td><td>NO</td></tr>",count,hwa,ip);
			}
			count++;
			fwrite(outBuff,strlen(outBuff),1,fp2);
		}
		//end luodp for webui  
	}
		fclose(fp2);
    }
    if (opt_v)
	printf(("Entries: %d\tSkipped: %d\tFound: %d\n"), entries, entries - showed, showed);

    if (!showed) {
	if (host[0] && !opt_a)
	    printf(("%s (%s) -- no entry\n"), name, host);
	else if (hw_set || host[0] || device[0]) {
	    printf(("arp: in %d entries no match found.\n"), entries);
	}
    }
    (void) fclose(fp);
    return (0);
}


static void usage(void)
{
    fprintf(stderr, ("Usage:\n  arp [-vn]  [<HW>] [-i <if>] [-a] [<hostname>]             <-Display ARP cache\n"));
    fprintf(stderr, ("  arp [-v]          [-i <if>] -d  <hostname> [pub][nopub]    <-Delete ARP entry\n"));
    fprintf(stderr, ("  arp [-vnD] [<HW>] [-i <if>] -f  [<filename>]              <-Add entry from file\n"));
    fprintf(stderr, ("  arp [-v]   [<HW>] [-i <if>] -s  <hostname> <hwaddr> [temp][nopub] <-Add entry\n"));
    fprintf(stderr, ("  arp [-v]   [<HW>] [-i <if>] -s  <hostname> <hwaddr> [netmask <nm>] pub  <-''-\n"));
    fprintf(stderr, ("  arp [-v]   [<HW>] [-i <if>] -Ds <hostname> <if> [netmask <nm>] pub      <-''-\n\n"));
    
    fprintf(stderr, ("        -a                       display (all) hosts in alternative (BSD) style\n"));
    fprintf(stderr, ("        -s, --set                set a new ARP entry\n"));
    fprintf(stderr, ("        -d, --delete             delete a specified entry\n"));
    fprintf(stderr, ("        -v, --verbose            be verbose\n"));
    fprintf(stderr, ("        -n, --numeric            don't resolve names\n"));
    fprintf(stderr, ("        -i, --device             specify network interface (e.g. eth0)\n"));
    fprintf(stderr, ("        -D, --use-device         read <hwaddr> from given device\n"));
    fprintf(stderr, ("        -A, -p, --protocol       specify protocol family\n"));
    fprintf(stderr, ("        -f, --file               read new entries from file or from /etc/ethers\n\n"));

    fprintf(stderr, ("  <HW>=Use '-H <hw>' to specify hardware address type. Default: %s\n"), DFLT_HW);
    fprintf(stderr, ("  List of possible hardware types (which support ARP):\n"));
    //print_hwlist(1); /* 1 = ARPable */
    exit(4);
}

int main(int argc, char **argv)
{
    int i, lop, what;
    struct option longopts[] =
    {
	{"verbose", 0, 0, 'v'},
	{"all", 0, 0, 'a'},
	{"delete", 0, 0, 'd'},
	{"file", 0, 0, 'f'},
	{"numeric", 0, 0, 'n'},
	{"set", 0, 0, 's'},
	{"protocol", 1, 0, 'A'},
	{"hw-type", 1, 0, 'H'},
	{"device", 1, 0, 'i'},
	{"help", 0, 0, 'h'},
	{"use-device", 0, 0, 'D'},
	{"symbolic", 0, 0, 'N'},
	{NULL, 0, 0, 0}
    };

#if I18N
    setlocale (LC_ALL, "");
    bindtextdomain("net-tools", "/usr/share/locale");
    textdomain("net-tools");
#endif

    /* Initialize variables... */
    if ((hw = get_hwtype(DFLT_HW)) == NULL) {
	fprintf(stderr, ("%s: hardware type not supported!\n"), DFLT_HW);
	return (-1);
    }
#if 0
    if ((ap = get_aftype(DFLT_AF)) == NULL) {
	fprintf(stderr, ("%s: address family not supported!\n"), DFLT_AF);
	return (-1);
    }
#endif

    what = 0;

    /* Fetch the command-line arguments. */
    /* opterr = 0; */
    while ((i = getopt_long(argc, argv, "A:H:adfp:nsei:t:vh?DNV", longopts, &lop)) != EOF)
	switch (i) {
	case 'a':
	    what = 1;
	    opt_a = 1;
	    break;
	case 'f':
	    what = 2;
	    break;
	case 'd':
	    what = 3;
	    break;
	case 's':
	    what = 4;
	    break;


	case 'e':
	    opt_e = 1;
	    break;
	case 'n':
	    opt_n = FLAG_NUM;
	    break;
	case 'D':
	    opt_D = 1;
	    break;
	case 'N':
	    opt_N = FLAG_SYM;
	    fprintf(stderr, ("arp: -N not yet supported.\n"));
	    break;
	case 'v':
	    opt_v = 1;
	    break;
#if 0

	case 'A':
	case 'p':

	    ap = get_aftype(optarg);
	    if (ap == NULL) {
		fprintf(stderr, ("arp: %s: unknown address family.\n"),
			optarg);
		exit(-1);
	    }
	    break;
#endif

	case 'H':
	case 't':
	    hw = get_hwtype(optarg);
	    if (hw == NULL) {
		fprintf(stderr, ("arp: %s: unknown hardware type.\n"),
			optarg);
		exit(-1);
	    }
	    hw_set = 1;
	    break;
	case 'i':
	    safe_strncpy(device, optarg, sizeof(device));
	    break;

	case '?':
	case 'h':
	default:
	    usage();
	}

#if 0
    if (ap->af != AF_INET) {
	fprintf(stderr, ("arp: %s: kernel only supports 'inet'.\n"),
		ap->name);
	exit(-1);
    }
#endif

    /* If not hw type specified get default */
    if(hw_set==0)
	if ((hw = get_hwtype(DFLT_HW)) == NULL) {
	  fprintf(stderr, ("%s: hardware type not supported!\n"), DFLT_HW);
	  return (-1);
	}

    if (hw->alen <= 0) {
	fprintf(stderr, ("arp: %s: hardware type without ARP support.\n"),
		hw->name);
	exit(-1);
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	exit(-1);
    }
    /* Now see what we have to do here... */
    switch (what) {
    case 0:
	opt_e = 1;
	what = arp_show(argv[optind]);
	break;

    case 1:			/* show an ARP entry in the cache */
	what = arp_show(argv[optind]);
	break;

    case 2:			/* process an EtherFile */
	what = arp_file(argv[optind] ? argv[optind] : "/etc/ethers");
	break;

    case 3:			/* delete an ARP entry from the cache */
	what = arp_del(&argv[optind]);
	break;

    case 4:			/* set an ARP entry in the cache */
	what = arp_set(&argv[optind]);
	break;

    default:
	usage();
    }

    exit(what);
}
