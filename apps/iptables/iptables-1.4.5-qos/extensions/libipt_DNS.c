/* Shared library add-on to iptables to add DNS support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <xtables.h>

struct ipt_dns_info { 
        unsigned int ip; 
	//int url_len;
        //char url[30]; 
};


static void DNS_help(void)
{
	printf(
"DNS target options:\n"
" --ip		the ip of A20 or gateway.\n\n");
}

static const struct option DNS_opts[] = {
	{ .name = "ip",        .has_arg = 1, .val = '1' },
	{ .name = NULL }
};

static void DNS_init(struct xt_entry_target *t)
{
	struct ipt_dns_info *dnsinfo = (struct ipt_dns_info *)t->data;


}

static int DNS_parse(int c, char **argv, int invert, unsigned int *flags,
                     const void *entry, struct xt_entry_target **target)
{
	struct ipt_dns_info *dnsinfo = (struct ipt_dns_info *)(*target)->data;
	const struct in_addr *ip;

    	xtables_check_inverse(optarg, &invert, &optind, 0);
	switch (c) {
	case '1':
		ip = xtables_numeric_to_ipaddr(argv[optind-1]);
		if (!ip)
			xtables_error(PARAMETER_PROBLEM, "Bad IP address \"%s\"\n",
				   argv[optind-1]);
		dnsinfo->ip = ip->s_addr;
			
		//printf("ip = %x\n", dnsinfo->ip);
		break;

	default:
		return 0;
	}

	return 1;
}

static void DNS_print(const void *ip, struct xt_entry_target *target,
                      int numeric)
{
	struct ipt_dns_info *dnsinfo = (struct ipt_dns_info *)target->data;

    //char ip_addr[20] = {0};
    char *ip_str = NULL;
    struct in_addr *ip_addr = (struct in_addr *)(&(dnsinfo->ip));
    ip_str = xtables_ipaddr_to_anyname(ip_addr);
	printf("%s", ip_str);
}

static void DNS_save(const void *ip, const struct xt_entry_target *target)
{

		printf("DNS save");
}

static struct xtables_target dns_tg_reg = {
    .name          = "DNS",
    .version       = XTABLES_VERSION,
    .family        = NFPROTO_IPV4,
    .size          = XT_ALIGN(sizeof(struct ipt_dns_info)),
    .userspacesize = XT_ALIGN(sizeof(struct ipt_dns_info)),
    .help          = DNS_help,
    .init          = DNS_init,
    .parse         = DNS_parse,
    .print         = DNS_print,
    .save          = DNS_save,
    .extra_opts    = DNS_opts,
};

void _init(void)
{
	xtables_register_target(&dns_tg_reg);
}
