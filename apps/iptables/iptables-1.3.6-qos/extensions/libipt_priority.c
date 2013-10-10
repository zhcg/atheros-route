/* Copyright (c) 2009 Atheros Communications, Inc.
 * All rights reserved 
 *
 * IP tables module for matching the value of the IPv4 priority field
 *
 * Author: Tos Xu    tosx@wicresoft.com    2008/12/16 
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_priority.h>


static void help(void) 
{
	printf(
"priority match v%s options\n"
"[!] --priority value		Match priority with numerical value\n"
"  		                This value can be in decimal (ex: 32)\n"
"               		or in hex (ex: 0x20)\n"
				, IPTABLES_VERSION
);
}

static struct option opts[] = {
	{ "priority", 1, 0, '1' },
	{ 0 }
};

static void
parse_priority(const char *s, struct ipt_classify_match_info *dinfo)
{
	unsigned int priority;
       
	if (string_to_number(s, 0, 255, &priority) == -1)
		exit_error(PARAMETER_PROBLEM,
			   "Invalid dscp `%s'\n", s);

    	dinfo->priority = (unsigned int) priority;
    	return;
}


static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_classify_match_info *dinfo
		= (struct ipt_classify_match_info *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "priority match: Only use --priority ONCE!");
		check_inverse(optarg, &invert, &optind, 0);
		parse_priority(argv[optind-1], dinfo);
		if (invert)
			dinfo->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
		           "priority match: Parameter --priority is required");
}

static void
print_priority(unsigned int priority, int invert, int numeric)
{
	if (invert)
		fputc('!', stdout);

 	printf("0x%02x ", priority);
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_classify_match_info *dinfo =
		(const struct ipt_classify_match_info *)match->data;
	printf("priority match ");
	print_priority(dinfo->priority, dinfo->invert, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_classify_match_info *dinfo =
		(const struct ipt_classify_match_info *)match->data;

	printf("--priority ");
	print_priority(dinfo->priority, dinfo->invert, 1);
}

static struct iptables_match priority = { 
	.next 		= NULL,
	.name 		= "priority",
	.version 	= IPTABLES_VERSION,
	.size 		= IPT_ALIGN(sizeof(struct ipt_classify_match_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_classify_match_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&priority);
}
