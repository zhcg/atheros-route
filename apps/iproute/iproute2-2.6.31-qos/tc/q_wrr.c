/*
 * q_wrr.c		WRR.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Martin Devera  <devik@cdi.cz>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"

enum
{
	TCA_WRR_UNSPEC,
	TCA_WRR_INIT,
	TCA_WRR_PARAMS,
	__TCA_WRR_MAX,
};

#define TCA_WRR_MAX (__TCA_WRR_MAX-1)

struct wrr_gopt 
{
	int total_queues;
};

struct wrr_class_opt
{
	int quantum;
	int handle;
};


static void explain(void)
{
	fprintf(stderr, "Usage: ... wrr\n");
}

static void explain2(void)
{
	fprintf(stderr, "Usage: ... wrr quantum q\n");
}

#define usage() return(-1)

static int wrr_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	int ok=0;
	struct wrr_gopt gopt;
	struct rtattr *tail;

	//printf("wrr_parse_opt. \n");
	while (argc > 0) {
		
		if (strcmp(*argv, "queues") == 0) {
			NEXT_ARG();
			if (get_size(&gopt.total_queues, *argv)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	//printf("wrr_parse_opt ok %d q=%d.\n",ok,gopt.total_queues);

	if (ok)
	{
		tail = NLMSG_TAIL(n);
		addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
		addattr_l(n, 2024, TCA_WRR_INIT, &gopt, sizeof(gopt));
		tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	}
	
	return 0;
}

static int wrr_parse_class_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	int ok=0; 
	struct wrr_class_opt copt;
	struct rtattr *tail;

	copt.handle = 0;
	copt.quantum = 0;

	//printf("wrr_parse_class_opt.\n");
	while (argc > 0) {
		if (strcmp(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_size(&copt.quantum, *argv)) {
				fprintf(stderr, "Illegal \"quantum\"\n");
				return -1;
			}
			ok++;
		}else if (strcmp(*argv, "nfmark") == 0) {
			NEXT_ARG();
			if (get_size(&copt.handle, *argv)) {
				fprintf(stderr, "Illegal \"nfmark\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "help") == 0) {
			explain2();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain2();
			return -1;
		}
		argc--; argv++;
	}

	//printf("wrr_parse_class_opt ok %d q=%d h:%x.\n",ok,copt.quantum,copt.handle);

	if (ok)
	{
		tail = NLMSG_TAIL(n);
		addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
		addattr_l(n, 2024, TCA_WRR_PARAMS, &copt, sizeof(copt));
		tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	}
	return 0;
}

static int wrr_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{

	struct rtattr *tb[TCA_OPTIONS-1];
	struct wrr_class_opt *copt;
	struct wrr_gopt *gopt;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_WRR_MAX, opt);

	if (tb[TCA_WRR_PARAMS]) {

	    copt = RTA_DATA(tb[TCA_WRR_PARAMS]);
	    if (RTA_PAYLOAD(tb[TCA_WRR_PARAMS])  < sizeof(*copt)) return -1;
	    fprintf(f, "quantum %u nfmark %u.", copt->quantum, copt->handle);

	}
	if (tb[TCA_WRR_INIT]) {

	    gopt = RTA_DATA(tb[TCA_WRR_INIT]);
	    if (RTA_PAYLOAD(tb[TCA_WRR_INIT])  < sizeof(*gopt)) return -1;
	    fprintf(f, "queues %u", gopt->total_queues);

	}

	return 0;
}

static int wrr_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	struct tc_stats *st;

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);

	if(st == NULL) return -1;

	fprintf(f, " bytes: %llu packets: %u \n", 
		st->bytes,st->packets);

	return 0;	

}

struct qdisc_util wrr_qdisc_util = {
	.id 		= "wrr",
	.parse_qopt	= wrr_parse_opt,
	.print_qopt	= wrr_print_opt,
	.print_xstats 	= wrr_print_xstats,
	.parse_copt	= wrr_parse_class_opt,
	.print_copt	= wrr_print_opt,
};

