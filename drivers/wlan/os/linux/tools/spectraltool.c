/*
 * Copyright (c) 2009, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>
#include <stdio.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <stdlib.h>


#include "if_athioctl.h"
#define _LINUX_TYPES_H
/*
 * Provide dummy defs for kernel types whose definitions are only
 * provided when compiling with __KERNEL__ defined.
 * This is required because ah_internal.h indirectly includes
 * kernel header files, which reference these data types.
 */
#define __be64 u_int64_t
#define __le64 u_int64_t
#define __be32 u_int32_t
#define __le32 u_int32_t
#define __be16 u_int16_t
#define __le16 u_int16_t
#define __be8  u_int8_t
#define __le8  u_int8_t

#include "ah.h"
#include "spectral_ioctl.h"
#include "ah_devid.h"
#include "ah_internal.h"
#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "dfs_ioctl.h"

#ifndef ATH_DEFAULT
#define	ATH_DEFAULT	"wifi0"
#endif
#ifndef ATH_PORT
#define	ATH_PORT	8001
#endif

struct spectralhandler {
	int	s;
	struct ath_diag atd;
};

int sd, newsd;
struct sockaddr_in client_addr;
socklen_t client_addr_len;
static int spectralInetStart(int dgram)
{
    struct sockaddr_in my_addr;
    int addrlen, on = 1, nrecv;
    char buf[32];

    if (dgram)
        sd = socket(PF_INET, SOCK_DGRAM, 0);
    else
        sd = socket(PF_INET, SOCK_STREAM, 0);
/*
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        printf("socket option failed\n");
        exit (0);
    }
*/

    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(ATH_PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sd, (struct sockaddr *)&my_addr, sizeof(my_addr));

    if (dgram) {
        printf("waiting for udp fft client\n");
        client_addr_len = sizeof(client_addr);
        nrecv = recvfrom(sd, buf, 32, 0, (struct sockaddr *)&client_addr, &client_addr_len); /* just for sync, wait until client connects to us */
        if (nrecv == -1) {
            perror("recv");
            return 1;
        }
        printf("connected to fft client %d\n", nrecv);
    }
    else {
        listen(sd, 10);

        printf("waiting for tcp fft client\n");
        addrlen = sizeof(struct sockaddr_in);
        if ((newsd = accept(sd, (struct sockaddr *)&client_addr, &addrlen)) == -1) {
            perror("accept");
            return 1;
        }
        printf("connected to fft client %d %d\n", nrecv, client_addr.sin_addr.s_addr);
    }

    return 0;
}
static int spectralInetStop(int dgram)
{
    if (!dgram)
        close(newsd);
    close(sd);
}

static int spectralNetlinkListen(int num_samples, int dgram)
{
    #define MAX_PAYLOAD 1024  /* maximum payload size*/
#ifndef NETLINK_ATHEROS
    #define NETLINK_ATHEROS 17
#endif
    struct sockaddr_nl src_addr;
    socklen_t fromlen;
    struct nlmsghdr *nlh = NULL;
    int sock_fd, read_bytes;
    struct msghdr msg;
    char buf[256];

    sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_ATHEROS);
    if (sock_fd < 0) {
            printf("socket errno=%d\n", sock_fd);
            return sock_fd;
    }
     memset(&src_addr, 0, sizeof(src_addr));
     src_addr.nl_family = PF_NETLINK;
     src_addr.nl_pid = getpid();  /* self pid */
     /* interested in group 1<<0 */
     src_addr.nl_groups = 1;
    
    if(read_bytes=bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        if (read_bytes < 0)
                        perror("bind(netlink)");
            printf("BIND errno=%d\n", read_bytes);
            return read_bytes;
    }

    printf("Waiting for data from kernel\n");

    while (num_samples--) {
        fromlen = sizeof(src_addr);
        read_bytes = recvfrom(sock_fd, buf, sizeof(buf), MSG_WAITALL,
                        (struct sockaddr *) &src_addr, &fromlen);
        printf("read_bytes=%d\n", read_bytes);

        if (read_bytes < 0) {
            perror("recvfrom(netlink)");
            close(sock_fd);
            return read_bytes;
        } else {
            ssize_t nsend;
            nlh = (struct nlmsghdr *) buf;
            printf(" Received message payload: %d-%d bytes %s\n",read_bytes, NLMSG_PAYLOAD(nlh,0), NLMSG_DATA(nlh));
            if (dgram) {
                nsend = sendto(sd, NLMSG_DATA(nlh), NLMSG_PAYLOAD(nlh,0), 0, (struct sockaddr *)&client_addr, client_addr_len);
                if (nsend == -1) 
                    perror("send");
            }
            else {
                nsend = send(newsd, NLMSG_DATA(nlh), NLMSG_PAYLOAD(nlh,0), 0);
                if (nsend == -1) 
                    perror("send");
            }
        }
    }
    close(sock_fd);
    return 0;
}

static int spectralListen(struct spectralhandler *spectral, int num_samples, int sock_mode)
{
    int rc;
    rc = spectralInetStart(sock_mode); /* 0 = tcp, 1 = udp */
    if (rc) return rc;

    rc = spectralNetlinkListen(num_samples, sock_mode);
    spectralInetStop(sock_mode);
    return rc;
}

static int
spectralIsEnabled(struct spectralhandler *spectral)
{
    u_int32_t result=0;
    struct ifreq ifr;

    spectral->atd.ad_id = SPECTRAL_IS_ENABLED | ATH_DIAG_DYN;
    spectral->atd.ad_in_data = NULL;
    spectral->atd.ad_in_size = 0;
    spectral->atd.ad_out_data = (void *) &result;
    spectral->atd.ad_out_size = sizeof(u_int32_t);
    strcpy(ifr.ifr_name, spectral->atd.ad_name);
    ifr.ifr_data = (caddr_t) &spectral->atd;
    if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
          err(1, spectral->atd.ad_name);
    return(result);
}
static int
spectralIsActive(struct spectralhandler *spectral)
{
    u_int32_t result=0;
    struct ifreq ifr;

    spectral->atd.ad_id = SPECTRAL_IS_ACTIVE | ATH_DIAG_DYN;
    spectral->atd.ad_in_data = NULL;
    spectral->atd.ad_in_size = 0;
    spectral->atd.ad_out_data = (void *) &result;
    spectral->atd.ad_out_size = sizeof(u_int32_t);
    strcpy(ifr.ifr_name, spectral->atd.ad_name);
    ifr.ifr_data = (caddr_t) &spectral->atd;
    if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
          err(1, spectral->atd.ad_name);
    return(result);
}
static int
spectralStartScan(struct spectralhandler *spectral)
{
	u_int32_t result;
    struct ifreq ifr;

	spectral->atd.ad_id = SPECTRAL_ACTIVATE_SCAN | ATH_DIAG_DYN;
	spectral->atd.ad_out_data = NULL;
	spectral->atd.ad_out_size = 0;
	spectral->atd.ad_in_data = (void *) &result;
	spectral->atd.ad_in_size = sizeof(u_int32_t);
        strcpy(ifr.ifr_name, spectral->atd.ad_name);
        ifr.ifr_data = (caddr_t)&spectral->atd;
	if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
		err(1, spectral->atd.ad_name);
	return 0;
}
static int
spectralStartClassifyScan(struct spectralhandler *spectral)
{
	u_int32_t result;
    struct ifreq ifr;

	spectral->atd.ad_id = SPECTRAL_CLASSIFY_SCAN | ATH_DIAG_DYN;
	spectral->atd.ad_out_data = NULL;
	spectral->atd.ad_out_size = 0;
	spectral->atd.ad_in_data = (void *) &result;
	spectral->atd.ad_in_size = sizeof(u_int32_t);
        strcpy(ifr.ifr_name, spectral->atd.ad_name);
        ifr.ifr_data = (caddr_t)&spectral->atd;
	if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
		err(1, spectral->atd.ad_name);
	return 0;
}

static int
spectralStopScan(struct spectralhandler *spectral)
{
	u_int32_t result;
    struct ifreq ifr;

	spectral->atd.ad_id = SPECTRAL_STOP_SCAN | ATH_DIAG_DYN;
	spectral->atd.ad_out_data = NULL;
	spectral->atd.ad_out_size = 0;
	spectral->atd.ad_in_data = (void *) &result;
	spectral->atd.ad_in_size = sizeof(u_int32_t);
        strcpy(ifr.ifr_name, spectral->atd.ad_name);
        ifr.ifr_data = (caddr_t)&spectral->atd;
	if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
		err(1, spectral->atd.ad_name);
	return 0;
}

static int
spectralSetDebugLevel(struct spectralhandler *spectral, u_int32_t level)
{
	u_int32_t result;
    struct ifreq ifr;

	spectral->atd.ad_id = SPECTRAL_SET_DEBUG_LEVEL | ATH_DIAG_IN;
	spectral->atd.ad_out_data = NULL;
	spectral->atd.ad_out_size = 0;
	spectral->atd.ad_in_data = (void *) &level;
	spectral->atd.ad_in_size = sizeof(u_int32_t);
        strcpy(ifr.ifr_name, spectral->atd.ad_name);
        ifr.ifr_data = (caddr_t)&spectral->atd;
	if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
		err(1, spectral->atd.ad_name);
	return 0;
}

static void
spectralGetThresholds(struct spectralhandler *spectral, HAL_SPECTRAL_PARAM *sp)
{
    struct ifreq ifr;
	spectral->atd.ad_id = SPECTRAL_GET_CONFIG | ATH_DIAG_DYN;
	spectral->atd.ad_out_data = (void *) sp;
	spectral->atd.ad_out_size = sizeof(HAL_SPECTRAL_PARAM);
    strcpy(ifr.ifr_name, spectral->atd.ad_name);
    ifr.ifr_data = (caddr_t)&spectral->atd;
	if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
		err(1, spectral->atd.ad_name);
}


#if 0
static void
spectralGetClassifierParams(struct spectralhandler *spectral, SPECTRAL_CLASSIFIER_PARAMS *sp)
{
    struct ifreq ifr;
	spectral->atd.ad_id = SPECTRAL_GET_CLASSIFIER_CONFIG | ATH_DIAG_DYN;
	spectral->atd.ad_out_data = (void *) sp;
	spectral->atd.ad_out_size = sizeof(SPECTRAL_CLASSIFIER_PARAMS);
    strcpy(ifr.ifr_name, spectral->atd.ad_name);
    ifr.ifr_data = (caddr_t)&spectral->atd;
	if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
		err(1, spectral->atd.ad_name);
}
#endif
static int
spectralEACS(struct spectralhandler *spectral)
{
	u_int32_t result;
        struct ifreq ifr;

	spectral->atd.ad_id = SPECTRAL_EACS | ATH_DIAG_DYN;
	spectral->atd.ad_out_data = NULL;
	spectral->atd.ad_out_size = 0;
	spectral->atd.ad_in_data = (void *) &result;
	spectral->atd.ad_in_size = sizeof(u_int32_t);
        strcpy(ifr.ifr_name, spectral->atd.ad_name);
        ifr.ifr_data = (caddr_t)&spectral->atd;
	if (ioctl(spectral->s, SIOCGATHEACS, &ifr) < 0)
		err(1, spectral->atd.ad_name);
	return 0;
}
void
spectralset(struct spectralhandler *spectral, int op, u_int32_t param)
{
	HAL_SPECTRAL_PARAM sp;
    struct ifreq ifr;

	sp.ss_period = HAL_PHYERR_PARAM_NOVAL;
	sp.ss_count = HAL_PHYERR_PARAM_NOVAL;
	sp.ss_fft_period = HAL_PHYERR_PARAM_NOVAL;
	sp.ss_short_report = HAL_PHYERR_PARAM_NOVAL;

	switch(op) {
	case SPECTRAL_PARAM_FFT_PERIOD:
		sp.ss_fft_period = param;
		break;
	case SPECTRAL_PARAM_SCAN_PERIOD:
		sp.ss_period = param;
		break;
	case SPECTRAL_PARAM_SHORT_REPORT:
	        if (param)
                    sp.ss_short_report = AH_TRUE;
                else 
                    sp.ss_short_report = AH_FALSE;
                printf("short being set to %d param %d\n", sp.ss_short_report, param);                       
		break;
	case SPECTRAL_PARAM_SCAN_COUNT:
		sp.ss_count = param;
		break;
	}
	spectral->atd.ad_id = SPECTRAL_SET_CONFIG | ATH_DIAG_IN;
	spectral->atd.ad_out_data = NULL;
	spectral->atd.ad_out_size = 0;
	spectral->atd.ad_in_data = (void *) &sp;
	spectral->atd.ad_in_size = sizeof(HAL_SPECTRAL_PARAM);
        strcpy(ifr.ifr_name, spectral->atd.ad_name);
        ifr.ifr_data = (caddr_t) &spectral->atd;

	if (ioctl(spectral->s, SIOCGATHPHYERR, &ifr) < 0)
		err(1, spectral->atd.ad_name);
}

static void
usage(void)
{
	const char *msg = "\
Usage: spectraltool [-i wifiX] [cmd] [cmd_parameter]\n\
    <cmd> = startscan, stopscan, startclassifscan, eacs do not require a param\n\
    <cmd> = fftperiod, period, short, count, debug listen require a param\n\
    <cmd> = -h : print this usage message\n\
    period    x, 0 <= x <= 255\n\
        configure the time duration between successive spectral scan mode entry points\n\
    count     x, 0 <= x <= 128\n\
        configure the number of times the chip will enter spectral scan mode.\n\
        if set to 128, spectral scan will continue indefinitely until software\n\
        explicitly clears the active bit\n\
    fftperiod x, 0 <= x <= 8\n\
        skip interval for FFT reports. report once every ((fftperiod+1)*4)\n\
        microseconds when short reports are disabled\n\
    short    [0|1]\n\
        configure short report mode - the amount of time chip stays in spectral\n\
        scan mode. if enabled (1), spectral scan mode will last 4us, else 204us\n\
    debug    x, 0 <= x <= 3\n\
    listen <num_samples> [tcp]\n";

	fprintf(stderr, "%s", msg);
}

int
main(int argc, char *argv[])
{
#define	streq(a,b)	(strcasecmp(a,b) == 0)
	struct spectralhandler spectral;
	HAL_REVS revs;
    struct ifreq ifr;

	memset(&spectral, 0, sizeof(spectral));
	spectral.s = socket(AF_INET, SOCK_DGRAM, 0);
	if (spectral.s < 0)
		err(1, "socket");
	if (argc > 1 && strcmp(argv[1], "-i") == 0) {
		if (argc < 2) {
			fprintf(stderr, "%s: missing interface name for -i\n",
				argv[0]);
			exit(-1);
		}
		strncpy(spectral.atd.ad_name, argv[2], sizeof (spectral.atd.ad_name));
		argc -= 2, argv += 2;
	} else
		strncpy(spectral.atd.ad_name, ATH_DEFAULT, sizeof (spectral.atd.ad_name));


	if (argc >= 2) {
		if(streq(argv[1], "fftperiod")) {
			spectralset(&spectral, SPECTRAL_PARAM_FFT_PERIOD, (u_int16_t) atoi(argv[2]));
		} else if (streq(argv[1], "period")) {
			spectralset(&spectral, SPECTRAL_PARAM_SCAN_PERIOD, (u_int16_t) atoi(argv[2]));
		} else if (streq(argv[1], "short")) {
			spectralset(&spectral, SPECTRAL_PARAM_SHORT_REPORT, (u_int16_t) atoi(argv[2]));
		} else if (streq(argv[1], "count")) {
			spectralset(&spectral, SPECTRAL_PARAM_SCAN_COUNT, (u_int16_t) atoi(argv[2]));
		} else if (streq(argv[1], "startscan")) {
			spectralStartScan(&spectral);
		} else if (streq(argv[1], "startclassifyscan")) {
			spectralStartClassifyScan(&spectral);
		} else if (streq(argv[1], "stopscan")) {
			spectralStopScan(&spectral);
		} else if (streq(argv[1], "debug")) {
                        spectralSetDebugLevel(&spectral, (u_int32_t)atoi(argv[2]));
		} else if (streq(argv[1],"-h")) {
			usage();
		} else if (streq(argv[1],"listen")) {
		    if (argc > 3 && streq(argv[3],"tcp"))
		        return spectralListen(&spectral, atoi(argv[2]), 0);
                    else
		        return spectralListen(&spectral, atoi(argv[2]), 1);
		} else if (streq(argv[1],"eacs")) {
		    return spectralEACS(&spectral);
                } else {
                    printf("Invalid command option used for spectraltool\n");
                    usage();
                }
	} else if (argc == 1) {
		HAL_SPECTRAL_PARAM sp;
                int val=0;
		printf ("SPECTRAL PARAMS\n");
                val = spectralIsEnabled(&spectral);
                printf("Spectral scan is %s\n", (val) ? "enabled": "disabled");
                val = spectralIsActive(&spectral);
                printf("Spectral scan is %s\n", (val) ? "active": "inactive");
		spectralGetThresholds(&spectral, &sp);
		printf ("fft_period:  %d\n",sp.ss_fft_period);
		printf ("ss_period: %d\n",sp.ss_period);
		printf ("ss_count: %d\n",sp.ss_count);
		printf ("short report: %s\n",(sp.ss_short_report) ? "yes":"no");
	} else {
		usage ();
	}
	return 0;
}
