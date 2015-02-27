/******************************************************************************
 ******************************************************************************/


#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <xtables.h>

struct xt_dnsurl_info {
	int enforce_dns;
        int url_len;
        char url[30];
};

/* Function which prints out usage message. */
static void help(void)
{
    printf(
    "  --url url          DNS  Match url\n"
    );
}

static struct option opts[] = {
    { "url",   1, 0, '1' },
    {0}
};

/* Initialize the match. */
static void init(struct xt_entry_match *m)
{
	struct xt_dnsurl_info *info = (struct xt_dnsurl_info *)m->data;

	info->enforce_dns = 0;
	//printf("here is init\n");
}

static void print_dnsurl(const struct xt_dnsurl_info *info)
{
    int i;
		printf("%s", info->url[i]);
}

/* Function which parses command options; returns true if it
   ate an option */
static int parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
      struct xt_entry_match **match)
{
	struct xt_dnsurl_info *info = (struct xt_dnsurl_info *)(*match)->data;

    xtables_check_inverse(optarg, &invert, &optind, 0);
    if (invert) xtables_error(PARAMETER_PROBLEM, "Sorry, you can't have an inverted comment");

//	printf("dnsurl parse\n");

	if (c == '1')
	{
		strcpy(info->url, argv[optind-1]);
		info->url_len = strlen(argv[optind-1]);
		if(!strcmp(argv[optind-1],"enforcedns"))
		{
			info->enforce_dns = 1;
		}
//		printf("url = %s\n",info->url);
//		printf("url_len = %d\n",info->url_len);
	}
	else
	{
		return 0;
	}
	
	if (*flags)
		xtables_error(PARAMETER_PROBLEM, "multiurl can only have one option");

	*flags = 2;


	return 1;
}

/* Final check; must specify something. */
static void final_check(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM, "dnsurl expection an option --url");
}

/* Prints out the matchinfo. */
static void print(const void *ip,
      const struct xt_entry_match *match,
      int numeric)
{

    const struct xt_dnsurl_info *info = (const struct xt_dnsurl_info *)match->data;

    printf("dnsurl ");
	printf("--url ");
	//print_dnsurl(info);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_dnsurl_info *info = (const struct xt_dnsurl_info *)match->data;
	print_dnsurl(info);
}

static struct xtables_match dnsurl = { 
	.next		= NULL,
	.name		= "dnsurl",
	.revision	= 0,
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size			= XT_ALIGN(sizeof(struct xt_dnsurl_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_dnsurl_info)),
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
	xtables_register_match(&dnsurl);
}

