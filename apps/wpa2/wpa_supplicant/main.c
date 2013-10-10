/*
 * WPA Supplicant / main() function for UNIX like OSes and MinGW
 * Copyright (c) 2003-2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"
#ifdef __linux__
#include <fcntl.h>
#endif /* __linux__ */

#include "common.h"
#include "wpa_supplicant_i.h"
#include "topology.h"


extern const char *wpa_supplicant_version;
#ifdef MODIFIED_BY_SONY
extern const char *modified_by_sony_version;
#endif /* MODIFIED_BY_SONY */
extern const char *wpa_supplicant_license;
#ifndef CONFIG_NO_STDOUT_DEBUG
extern const char *wpa_supplicant_full_license1;
extern const char *wpa_supplicant_full_license2;
extern const char *wpa_supplicant_full_license3;
extern const char *wpa_supplicant_full_license4;
extern const char *wpa_supplicant_full_license5;
#endif /* CONFIG_NO_STDOUT_DEBUG */

extern struct wpa_driver_ops *wpa_supplicant_drivers[];


static void usage(void)
{
	int i;
        #if 0   /* WAS */
	printf("%s\n\n%s\n"
	       "usage:\n"
	       "  wpa_supplicant [-BddehLqquvwW] [-P<pid file>] "
	       "[-g<global ctrl>] \\\n"
	       "        -i<ifname> -c<config file> [-C<ctrl>] [-D<driver>] "
	       "[-p<driver_param>] \\\n"
	       "        [-b<br_ifname> [-N -i<ifname> -c<conf> [-C<ctrl>] "
	       "[-D<driver>] \\\n"
	       "        [-p<driver_param>] [-b<br_ifname>] ...]\n"
	       "\n"
	       "drivers:\n",
	       wpa_supplicant_version, wpa_supplicant_license);
        #else
	printf("%s\n\n%s\n"
	       "usage:\n"
	       "  wpa_supplicant [-BddehLqquvwW] [-P<pid file>] "
	       "[-g<global ctrl>] \\\n"
	       "        [-C<ctrl>] \\\n"
	       "\n"
	       "drivers:\n",
	       wpa_supplicant_version, wpa_supplicant_license);
        #endif

	for (i = 0; wpa_supplicant_drivers[i]; i++) {
		printf("  %s = %s\n",
		       wpa_supplicant_drivers[i]->name,
		       wpa_supplicant_drivers[i]->desc);
	}

#ifndef CONFIG_NO_STDOUT_DEBUG
	printf("options:\n"
               #if 0    /* WAS */
	       "  -b = optional bridge interface name\n"
               #endif  /* WAS */
	       "  -B = run daemon in the background\n"
               #if 0    /* WAS */
	       "  -c = Configuration file\n"
               #endif  /* WAS */
	       "  -C = ctrl_interface parameter (only used if -c is not)\n"
               #if 0    /* WAS */
	       "  -i = interface name\n"
               #endif  /* WAS */
	       "  -d = increase debugging verbosity (-dd even more)\n"
               #if 0    /* WAS */
	       "  -D = driver name\n"
               #endif  /* WAS */
	       "  -g = global ctrl_interface\n"
	       "  -K = include keys (passwords, etc.) in debug output\n"
	       "  -t = include timestamp in debug messages\n"
	       "  -h = show this help text\n"
	       "  -L = show license (GPL and BSD)\n");
	printf("  -p = driver parameters\n"
	       "  -P = PID file\n"
	       "  -q = decrease debugging verbosity (-qq even less)\n"
#ifdef CONFIG_CTRL_IFACE_DBUS
	       "  -u = enable DBus control interface\n"
#endif /* CONFIG_CTRL_IFACE_DBUS */
	       "  -v = show version\n"
	       "  -w = wait for interface to be added, if needed\n"
	       "  -W = wait for a control interface monitor before starting\n"
#ifdef WPS_OPT_NFC
	       "  -n = nfc interface name\n"
#endif /* WPS_OPT_NFC */
               #if 0    /* WAS */
	       "  -N = start describing new interface");
               #endif  /* WAS */
               "\n");

        #if 0   /* WAS */
	printf("example:\n"
	       "  wpa_supplicant -Dwext -iwlan0 -c/etc/wpa_supplicant.conf\n");
        #endif
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


static void license(void)
{
#ifndef CONFIG_NO_STDOUT_DEBUG
	printf("%s\n\n%s%s%s%s%s\n",
	       wpa_supplicant_version,
	       wpa_supplicant_full_license1,
	       wpa_supplicant_full_license2,
	       wpa_supplicant_full_license3,
	       wpa_supplicant_full_license4,
	       wpa_supplicant_full_license5);
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


static void wpa_supplicant_fd_workaround(void)
{
#ifdef __linux__
	int s, i;
	/* When started from pcmcia-cs scripts, wpa_supplicant might start with
	 * fd 0, 1, and 2 closed. This will cause some issues because many
	 * places in wpa_supplicant are still printing out to stdout. As a
	 * workaround, make sure that fd's 0, 1, and 2 are not used for other
	 * sockets. */
	for (i = 0; i < 3; i++) {
		s = open("/dev/null", O_RDWR);
		if (s > 2) {
			close(s);
			break;
		}
	}
#endif /* __linux__ */
}


int main(int argc, char *argv[])
{
	int c;
        #if 0   /* WAS */
	struct wpa_interface *ifaces, *iface;
	int iface_count;
        #endif  /* WAS */
        int exitcode = 1;
	struct wpa_params params;
	struct wpa_global *global;

#ifdef MODIFIED_BY_SONY
#ifndef CONFIG_NATIVE_WINDOWS
	setvbuf(stdout, 0, _IOLBF, 0);
	setvbuf(stderr, 0, _IOLBF, 0);
#else /* CONFIG_NATIVE_WINDOWS */
	setbuf(stdout, 0);
	setbuf(stderr, 0);
#endif /* CONFIG_NATIVE_WINDOWS */
#endif /* MODIFIED_BY_SONY */

	if (os_program_init())
		return 1;

	os_memset(&params, 0, sizeof(params));
	params.wpa_debug_level = MSG_INFO;

        #if 0   /* WAS */
	iface = ifaces = os_zalloc(sizeof(struct wpa_interface));
	if (ifaces == NULL)
		return 1;
	iface_count = 1;
        #endif  /* WAS */

	wpa_supplicant_fd_workaround();

	for (;;) {
#ifndef WPS_OPT_NFC
                #if 0   /* WAS */
		c = getopt(argc, argv, "b:Bc:C:D:dg:hi:KLNp:P:qtuvwW");
                #else
		c = getopt(argc, argv, "BC:dg:hKLp:P:qtuvwW");
                #endif
#else /* WPS_OPT_NFC */
                #if 0   /* WAS */
		c = getopt(argc, argv, "b:Bc:C:D:dg:hi:KLn:Np:P:qtuvwW");
                #else
		c = getopt(argc, argv, "BC:dg:hKLn:p:P:qtuvwW");
                #endif
#endif /* WPS_OPT_NFC */
		if (c < 0)
			break;
		switch (c) {
        #if 0   /* WAS */
		case 'b':
			iface->bridge_ifname = optarg;
			break;
        #endif  /* WAS */
		case 'B':
			params.daemonize++;
			break;
        #if 0   /* WAS */
		case 'c':
			iface->confname = optarg;
			break;
        #endif  /* WAS */
        #if 0   /* WAS */
		case 'C':
			iface->ctrl_interface = optarg;
			break;
        #endif  /* WAS */
        #if 0   /* WAS */
		case 'D':
			iface->driver = optarg;
			break;
        #endif  /* WAS */
		case 'd':
#ifdef CONFIG_NO_STDOUT_DEBUG
			printf("Debugging disabled with "
			       "CONFIG_NO_STDOUT_DEBUG=y build time "
			       "option.\n");
			goto out;
#else /* CONFIG_NO_STDOUT_DEBUG */
			params.wpa_debug_level--;
			break;
#endif /* CONFIG_NO_STDOUT_DEBUG */
		case 'g':
			params.ctrl_interface = optarg;
			break;
		case 'h':
			usage();
			exitcode = 0;
			goto out;
        #if 0   /* WAS */
		case 'i':
			iface->ifname = optarg;
			break;
        #endif  /* WAS */
		case 'K':
			params.wpa_debug_show_keys++;
			break;
		case 'L':
			license();
			exitcode = 0;
			goto out;
        #if 0   /* WAS */
		case 'p':
			iface->driver_param = optarg;
			break;
        #endif  /* WAS */
		case 'P':
			os_free(params.pid_file);
			params.pid_file = os_rel2abs_path(optarg);
			break;
		case 'q':
			params.wpa_debug_level++;
			break;
		case 't':
			params.wpa_debug_timestamp++;
			break;
#ifdef CONFIG_CTRL_IFACE_DBUS
		case 'u':
			params.dbus_ctrl_interface = 1;
			break;
#endif /* CONFIG_CTRL_IFACE_DBUS */
		case 'v':
			printf("%s\n", wpa_supplicant_version);
#ifdef MODIFIED_BY_SONY
			printf("%s\n", modified_by_sony_version);
#endif /* MODIFIED_BY_SONY */
			exitcode = 0;
			goto out;
		case 'w':
			params.wait_for_interface++;
			break;
		case 'W':
			params.wait_for_monitor++;
			break;
#ifdef WPS_OPT_NFC
		case 'n':
			iface->nfcname = optarg;
			break;
#endif /* WPS_OPT_NFC */
        #if 0   /* WAS */
		case 'N':
			iface_count++;
			iface = os_realloc(ifaces, iface_count *
					   sizeof(struct wpa_interface));
			if (iface == NULL)
				goto out;
			ifaces = iface;
			iface = &ifaces[iface_count - 1]; 
			os_memset(iface, 0, sizeof(*iface));
			break;
        #endif  /* WAS */
		default:
			usage();
			exitcode = 0;
			goto out;
		}
	}

	exitcode = 0;
	global = wpa_supplicant_init(&params);
	if (global == NULL) {
		printf("Failed to initialize wpa_supplicant\n");
		exitcode = 1;
		goto out;
	}

        #if 0   /* WAS */
	for (i = 0; exitcode == 0 && i < iface_count; i++) {
		if ((ifaces[i].confname == NULL &&
		     ifaces[i].ctrl_interface == NULL) ||
		    ifaces[i].ifname == NULL) {
			if (iface_count == 1 && (params.ctrl_interface ||
						 params.dbus_ctrl_interface))
				break;
			usage();
			exitcode = 1;
			break;
		}
		if (wpa_supplicant_add_iface(global, &ifaces[i]) == NULL)
			exitcode = 1;
	}
        #endif  /* WAS */

        /* One or more topology files may be specified 
         * (one should be enough, but...)
         */
        if (argv[optind] == NULL) {
                printf("Need topology file as argument, or use -h for help\n");
                exitcode = 1;
                goto out;
        }
        if (wpa_supplicant_config_read_topology_files(global, argv+optind)) {
                exitcode = 1;
        }

	if (exitcode == 0)
		exitcode = wpa_supplicant_run(global);

	wpa_supplicant_deinit(global);

out:
        #if 0   /* WAS */
	os_free(ifaces);
        #endif  /* WAS */
	os_free(params.pid_file);

	os_program_deinit();

	return (exitcode != 0);
}
