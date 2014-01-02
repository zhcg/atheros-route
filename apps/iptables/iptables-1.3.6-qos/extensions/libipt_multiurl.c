/******************************************************************************
 *
 * Copyright (c) 2008-2008 TP-Link Technologies CO.,LTD.
 * All rights reserved.
 *
 * 文件名称:	libipt_multiurl.c.h
 * 版    本:	1.0
 * 摘    要:	Shared library add-on to iptables
 *
 * 作    者:	ZJin <zhongjin@tp-link.net>
 * 创建日期:	10/23/2008
 *
 * 修改历史:
----------------------------
 *
 ******************************************************************************/

/* for example */
/*
	iptables -A FORWARD -m multiurl --urls www.baidu.com,sina,163 -p tcp --dport 80 -j ACCEPT

	netfilter will find the urls in every http-GET pkt, if found, means matched!
*/

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <xtables.h>
/* To ensure that iptables compiles with an old kernel */
#include <linux/netfilter_ipv4/ipt_multiurl.h>


/* Function which prints out usage message. */
static void help(void)
{
    printf(
    "multiurl v%s options:\n"
    "  www.baidu.com,sina,gov            Match urls\n"
    , MULTIURL_VERSION);
}

static struct option opts[] = {
    { "urls",   1, 0, '1' },
    {0}
};

/* Initialize the match. */
static void init(struct xt_entry_match *m)
{
	struct xt_multiurl_info *info = (struct xt_multiurl_info *)m->data;

	info->url_count = 0;
	//printf("here is init\n");
}

static void print_multiurl(const struct xt_multiurl_info *info)
{
    int i;
    for (i = 0; i < info->url_count; i++)
	{
		printf("%s", info->urls[i]);
        if (i < info->url_count - 1) printf(",");
        if (i == info->url_count - 1) printf(" ");
    }
}

/*
	This function resolve the string "www.baidu.com,sina,163,xxx,sex", which multiurl point to it.
*/
static void parse_url(char *multiurl, struct xt_multiurl_info *info)
{
    char *next, *prev;
    int count = 0;

    prev = multiurl;
    next = multiurl;

	//printf("debug: before pase\n");

    do
	{
        if ((next = strchr(next, ',')) != NULL)
        {			
            *next++ = '\0';
        }

		if (strlen(prev) >= IPT_MULTIURL_STRLEN)
			xtables_error(PARAMETER_PROBLEM, "multiurl match: too long '%s'\n", prev);
		
		if (count >= IPT_MULTIURL_MAX_URL)
			xtables_error(PARAMETER_PROBLEM, "multiurl match: too many url specified (MAX = %d)\n", IPT_MULTIURL_MAX_URL);
		
		strncpy(info->urls[count], prev, strlen(prev));

		//printf("multiurl_debug: url_%d, %s\n", count, info->urls[count]);
		
        count++;
        prev = next;

		if ((next + 1) == 0)
		{
			break;
		}
    } while (next);

    info->url_count = count;
}

/* Function which parses command options; returns true if it
   ate an option */
static int parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
      struct xt_entry_match **match)
{
	struct xt_multiurl_info *info = (struct xt_multiurl_info *)(*match)->data;

    xtables_check_inverse(optarg, &invert, &optind, 0);
    if (invert) xtables_error(PARAMETER_PROBLEM, "Sorry, you can't have an inverted comment");

	if (c == '1')
	{
		parse_url(argv[optind - 1], info);
	}
	else
	{
		return 0;
	}
	
	if (*flags)
		xtables_error(PARAMETER_PROBLEM, "multiurl can only have one option");

	*flags = IPT_MULTIURL;

	return 1;
}

/* Final check; must specify something. */
static void final_check(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM, "multiurl expection an option --urls");
}

/* Prints out the matchinfo. */
static void print(const void *ip,
      const struct xt_entry_match *match,
      int numeric)
{

    const struct xt_multiurl_info *info = (const struct xt_multiurl_info *)match->data;

    printf("multiurl ");
	printf("--urls ");
	print_multiurl(info);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_multiurl_info *info = (const struct xt_multiurl_info *)match->data;
	print_multiurl(info);
}

static struct xtables_match multiurl = { 
	.next		= NULL,
	.name		= "multiurl",
	.revision	= 0,
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size			= XT_ALIGN(sizeof(struct xt_multiurl_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_multiurl_info)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	xtables_register_match(&multiurl);
}

