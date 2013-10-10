/*
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <asm/param.h>
#include "libbridge/libbridge.h"
#include "libbridge/brctl.h"
#include "busybox.h"

void br_dump_bridge_id(unsigned char *x)
{
	printf("%.2x%.2x.%.2x%.2x%.2x%.2x%.2x%.2x", x[0], x[1], x[2], x[3],
		   x[4], x[5], x[6], x[7]);
}

void br_show_timer(struct timeval *tv)
{
	printf("%4i.%.2i", (int) tv->tv_sec, (int) tv->tv_usec / 10000);
}

void br_dump_interface_list(struct bridge *br)
{
	char ifname[IFNAMSIZ];
	struct port *p;

	p = br->firstport;
	if (p != NULL) {
		printf("%s", if_indextoname(p->ifindex, ifname));
		p = p->next;
	}
	printf("\n");

	while (p != NULL) {
		printf("\t\t\t\t\t\t\t%s\n", if_indextoname(p->ifindex, ifname));
		p = p->next;
	}
}

void br_dump_port_info(struct port *p)
{
	char ifname[IFNAMSIZ];
	struct port_info *pi;

	pi = &p->info;

	printf("%s (%i)\n", if_indextoname(p->ifindex, ifname), p->index);
	printf(" port id\t\t%.4x\t\t\t", pi->port_id);
	printf("state\t\t\t%s\n", br_get_state_name(pi->state));
	printf(" designated root\t");
	br_dump_bridge_id((unsigned char *) &pi->designated_root);
	printf("\tpath cost\t\t%4i\n", pi->path_cost);

	printf(" designated bridge\t");
	br_dump_bridge_id((unsigned char *) &pi->designated_bridge);
	printf("\tmessage age timer\t");
	br_show_timer(&pi->message_age_timer_value);
	printf("\n designated port\t%.4x", pi->designated_port);
	printf("\t\t\tforward delay timer\t");
	br_show_timer(&pi->forward_delay_timer_value);
	printf("\n designated cost\t%4i", pi->designated_cost);
	printf("\t\t\thold timer\t\t");
	br_show_timer(&pi->hold_timer_value);
	printf("\n flags\t\t\t");
	if (pi->config_pending)
		printf("CONFIG_PENDING ");
	if (pi->top_change_ack)
		printf("TOPOLOGY_CHANGE_ACK ");
	printf("\n");
	printf("\n");
}

void br_dump_info(struct bridge *br)
{
	struct bridge_info *bri;
	struct port *p;

	bri = &br->info;

	printf("%s\n", br->ifname);
	if (!bri->stp_enabled) {
		printf(" STP is disabled for this interface\n");
		return;
	}

	printf(" bridge id\t\t");
	br_dump_bridge_id((unsigned char *) &bri->bridge_id);
	printf("\n designated root\t");
	br_dump_bridge_id((unsigned char *) &bri->designated_root);
	printf("\n root port\t\t%4i\t\t\t", bri->root_port);
	printf("path cost\t\t%4i\n", bri->root_path_cost);
	printf(" max age\t\t");
	br_show_timer(&bri->max_age);
	printf("\t\t\tbridge max age\t\t");
	br_show_timer(&bri->bridge_max_age);
	printf("\n hello time\t\t");
	br_show_timer(&bri->hello_time);
	printf("\t\t\tbridge hello time\t");
	br_show_timer(&bri->bridge_hello_time);
	printf("\n forward delay\t\t");
	br_show_timer(&bri->forward_delay);
	printf("\t\t\tbridge forward delay\t");
	br_show_timer(&bri->bridge_forward_delay);
	printf("\n ageing time\t\t");
	br_show_timer(&bri->ageing_time);
	printf("\t\t\tgc interval\t\t");
	br_show_timer(&bri->gc_interval);
	printf("\n hello timer\t\t");
	br_show_timer(&bri->hello_timer_value);
	printf("\t\t\ttcn timer\t\t");
	br_show_timer(&bri->tcn_timer_value);
	printf("\n topology change timer\t");
	br_show_timer(&bri->topology_change_timer_value);
	printf("\t\t\tgc timer\t\t");
	br_show_timer(&bri->gc_timer_value);
	printf("\n flags\t\t\t");
	if (bri->topology_change)
		printf("TOPOLOGY_CHANGE ");
	if (bri->topology_change_detected)
		printf("TOPOLOGY_CHANGE_DETECTED ");
	printf("\n");
	printf("\n");
	printf("\n");

	p = br->firstport;
	while (p != NULL) {
		br_dump_port_info(p);
		p = p->next;
	}
}


void br_cmd_addbr(struct bridge *br, char *brname, char *arg1)
{
	int err;

	if ((err = br_add_bridge(brname)) == 0)
		return;

	switch (err) {
	case EEXIST:
		fprintf(stderr, "device %s already exists; can't create "
				"bridge with the same name\n", brname);
		break;

	default:
		perror("br_add_bridge");
		break;
	}
}

void br_cmd_delbr(struct bridge *br, char *brname, char *arg1)
{
	int err;

	if ((err = br_del_bridge(brname)) == 0)
		return;

	switch (err) {
	case ENXIO:
		fprintf(stderr, "bridge %s doesn't exist; can't delete it\n", brname);
		break;

	case EBUSY:
		fprintf(stderr, "bridge %s is still up; can't delete it\n", brname);
		break;

	default:
		perror("br_del_bridge");
		break;
	}
}

void br_cmd_addif(struct bridge *br, char *ifname, char *arg1)
{
	int err;
	int ifindex;

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		fprintf(stderr, "interface %s does not exist!\n", ifname);
		return;
	}

	if ((err = br_add_interface(br, ifindex)) == 0)
		return;

	switch (err) {
	case EBUSY:
		fprintf(stderr, "device %s is already a member of a bridge; "
				"can't enslave it to bridge %s.\n", ifname, br->ifname);
		break;

	case ELOOP:
		fprintf(stderr, "device %s is a bridge device itself; "
				"can't enslave a bridge device to a bridge device.\n",
				ifname);
		break;

	default:
		perror("br_add_interface");
		break;
	}
}

void br_cmd_delif(struct bridge *br, char *ifname, char *arg1)
{
	int err;
	int ifindex;

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		fprintf(stderr, "interface %s does not exist!\n", ifname);
		return;
	}

	if ((err = br_del_interface(br, ifindex)) == 0)
		return;

	switch (err) {
	case EINVAL:
		fprintf(stderr, "device %s is not a slave of %s\n",
				ifname, br->ifname);
		break;

	default:
		perror("br_del_interface");
		break;
	}
}

void br_cmd_setageing(struct bridge *br, char *time, char *arg1)
{
	double secs;
	struct timeval tv;

	sscanf(time, "%lf", &secs);
	tv.tv_sec = secs;
	tv.tv_usec = 1000000 * (secs - tv.tv_sec);
	br_set_ageing_time(br, &tv);
}

void br_cmd_setbridgeprio(struct bridge *br, char *_prio, char *arg1)
{
	int prio;

	sscanf(_prio, "%i", &prio);
	br_set_bridge_priority(br, prio);
}

void br_cmd_setfd(struct bridge *br, char *time, char *arg1)
{
	double secs;
	struct timeval tv;

	sscanf(time, "%lf", &secs);
	tv.tv_sec = secs;
	tv.tv_usec = 1000000 * (secs - tv.tv_sec);
	br_set_bridge_forward_delay(br, &tv);
}

void br_cmd_setgcint(struct bridge *br, char *time, char *arg1)
{
	double secs;
	struct timeval tv;

	sscanf(time, "%lf", &secs);
	tv.tv_sec = secs;
	tv.tv_usec = 1000000 * (secs - tv.tv_sec);
	br_set_gc_interval(br, &tv);
}

void br_cmd_sethello(struct bridge *br, char *time, char *arg1)
{
	double secs;
	struct timeval tv;

	sscanf(time, "%lf", &secs);
	tv.tv_sec = secs;
	tv.tv_usec = 1000000 * (secs - tv.tv_sec);
	br_set_bridge_hello_time(br, &tv);
}

void br_cmd_setmaxage(struct bridge *br, char *time, char *arg1)
{
	double secs;
	struct timeval tv;

	sscanf(time, "%lf", &secs);
	tv.tv_sec = secs;
	tv.tv_usec = 1000000 * (secs - tv.tv_sec);
	br_set_bridge_max_age(br, &tv);
}

void br_cmd_setpathcost(struct bridge *br, char *arg0, char *arg1)
{
	int cost;
	struct port *p;

	if ((p = br_find_port(br, arg0)) == NULL) {
		fprintf(stderr, "can't find port %s in bridge %s\n", arg0,
				br->ifname);
		return;
	}

	sscanf(arg1, "%i", &cost);
	br_set_path_cost(p, cost);
}

void br_cmd_setportprio(struct bridge *br, char *arg0, char *arg1)
{
	int cost;
	struct port *p;

	if ((p = br_find_port(br, arg0)) == NULL) {
		fprintf(stderr, "can't find port %s in bridge %s\n", arg0,
				br->ifname);
		return;
	}

	sscanf(arg1, "%i", &cost);
	br_set_port_priority(p, cost);
}

void br_cmd_stp(struct bridge *br, char *arg0, char *arg1)
{
	int stp;

	stp = 0;
	if (!strcmp(arg0, "on") || !strcmp(arg0, "yes") || !strcmp(arg0, "1"))
		stp = 1;

	br_set_stp_state(br, stp);
}

void br_cmd_showstp(struct bridge *br, char *arg0, char *arg1)
{
	br_dump_info(br);
}

void br_cmd_show(struct bridge *br, char *arg0, char *arg1)
{
	printf("bridge name\tbridge id\t\tSTP enabled\tinterfaces\n");
	br = bridge_list;
	while (br != NULL) {
		printf("%s\t\t", br->ifname);
		br_dump_bridge_id((unsigned char *) &br->info.bridge_id);
		printf("\t%s\t\t", br->info.stp_enabled ? "yes" : "no");
		br_dump_interface_list(br);

		br = br->next;
	}
}

static int compare_fdbs(const void *_f0, const void *_f1)
{
	const struct fdb_entry *f0 = _f0;
	const struct fdb_entry *f1 = _f1;

#if 0
	if (f0->port_no < f1->port_no)
		return -1;

	if (f0->port_no > f1->port_no)
		return 1;
#endif

	return memcmp(f0->mac_addr, f1->mac_addr, 6);
}

void __dump_fdb_entry(struct fdb_entry *f)
{
	printf("%3i\t", f->port_no);
	printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\t",
		   f->mac_addr[0], f->mac_addr[1], f->mac_addr[2],
		   f->mac_addr[3], f->mac_addr[4], f->mac_addr[5]);
	printf("%s\t\t", f->is_local ? "yes" : "no");
	br_show_timer(&f->ageing_timer_value);
	printf("\n");
}

void br_cmd_showmacs(struct bridge *br, char *arg0, char *arg1)
{
	struct fdb_entry fdb[1024];
	int offset;

	printf("port\tmac addr\t\tlocal\tageing timer\n");

	offset = 0;
	while (1) {
		int i;
		int num;

		num = br_read_fdb(br, fdb, offset, 1024);
		if (!num)
			break;

		qsort(fdb, num, sizeof(struct fdb_entry), compare_fdbs);

		for (i = 0; i < num; i++)
			__dump_fdb_entry(fdb + i);

		offset += num;
	}
}

static struct command commands[] = {
	{0, 1, "addbr", br_cmd_addbr},
	{1, 1, "addif", br_cmd_addif},
	{0, 1, "delbr", br_cmd_delbr},
	{1, 1, "delif", br_cmd_delif},
	{1, 1, "setageing", br_cmd_setageing},
	{1, 1, "setbridgeprio", br_cmd_setbridgeprio},
	{1, 1, "setfd", br_cmd_setfd},
	{1, 1, "setgcint", br_cmd_setgcint},
	{1, 1, "sethello", br_cmd_sethello},
	{1, 1, "setmaxage", br_cmd_setmaxage},
	{1, 2, "setpathcost", br_cmd_setpathcost},
	{1, 2, "setportprio", br_cmd_setportprio},
	{0, 0, "show", br_cmd_show},
	{1, 0, "showmacs", br_cmd_showmacs},
	{1, 0, "showstp", br_cmd_showstp},
	{1, 1, "stp", br_cmd_stp},
};

struct command *br_command_lookup(char *cmd)
{
	int i;
	int numcommands;

	numcommands = sizeof(commands) / sizeof(commands[0]);

	for (i = 0; i < numcommands; i++)
		if (!strcmp(cmd, commands[i].name))
			return &commands[i];

	return NULL;
}



int brctl_main(int argc, char *argv[])
{
	int argindex;
	struct bridge *br;
	struct command *cmd;

	br_init();

	if (argc < 2)
		goto help;

	if ((cmd = br_command_lookup(argv[1])) == NULL) {
		fprintf(stderr, "never heard of command [%s]\n", argv[1]);
		goto help;
	}

	argindex = 2;
	br = NULL;
	if (cmd->needs_bridge_argument) {
		if (argindex >= argc) {
			fprintf(stderr,
					"this option requires a bridge name as argument\n");
			return 1;
		}

		br = br_find_bridge(argv[argindex]);

		if (br == NULL) {
			fprintf(stderr, "bridge %s doesn't exist!\n", argv[argindex]);
			return 1;
		}

		argindex++;
	}

	if (argc - argindex != cmd->num_string_arguments) {
		fprintf(stderr, "incorrect number of arguments for command\n");
		return 1;
	}

	cmd->func(br, argv[argindex], argv[argindex + 1]);

	return 0;

  help:
	bb_show_usage();
	return 1;
}
