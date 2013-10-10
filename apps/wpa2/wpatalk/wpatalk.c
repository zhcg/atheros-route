/*
 * wpatalk -- talk to wpa_supplicant or hostapd
 *
 * BASED UPON wpa_cli, whose header originally read:
 *
 * WPA Supplicant - command line interface for wpa_supplicant daemon
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
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


const char * const usage[] = {
"wpatalk -- talk to wpa_supplicant or hostapd",
" ",
"Usage: wpatalk [<option>...] {<interface>|<socketpath>} [<command>...]]",
" ",
"Options include: ",
"    -h | -help | --help :  print this help message to stdout and exit", 
"    -v  : more verbosity ",
"    -q  : less verbosity ",
"    -i  : interactive (do not abort on failed commands) ",
"    -noi : non-interactive (abort on failed commands) ",
"    -k  : keep going even if not connected or lost connection or errors ",
" ",
"The <interface> is the name of the wireless networking device (e.g. ath0) ",
"which a running instance of the daemon (wpa_supplicant or hostapd) ",
"is in charge of; more specifically, it is the name of the socket file ",
"which is used to communicate with the daemon. ",
"If just the simple interace name is given, a search is done of ",
"/var/run/hostapd and /var/run/wpa_supplicant to find the socket file ",
"named after the interface (and also thus determine which daemon is being ",
"talked to). ",
"If the form hostapd/<interface> or wpa_supplicant/<interface> is used, ",
"then the socket file is found beneath /var/run . ",
"If an absolute path is used then this path is used; the last directory ",
"name must however be either hostapd or wpa_supplicant. ",
"In the above, a different path given by WPA_CTRL_DIR env. var. ",
"will be used if present, instead of /var/run . ",
" ",
"Each <command> of the wpatalk invocation must be a single argument, ",
"e.g. it will be necessary from the shell to quote each command that ",
"contains more than one command argument word. ",
"With no command(s), an interactive session is entered, wherein ",
"each input line is a separate command. ",
"Whitespace at the beginning and end of interactive commands is stripped, ",
"and commands beginning with a sharp (#) are treated as comments. ",
"Within raw commands (see below) there must be exactly one space between ",
"each argument word of the raw command; ",
"builtin commands (see also below) may have arbitrary whitespace between ",
"command argument words. ",
" ",
"Commands beginning with an upper case letter are taken to be raw commands ",
"and are passed directly to the daemon. ",
"Refer to documentation per daemon on these commands. ",
" ",
"Other commands are assumed to be builtin commands, which include the ",
"following: ",
"  ",
"    configme [pin[=<pin>]] [upnp] [bssid=<macaddr>] [ssid={<text>|0x<hex>}] ",
"               -- use WPS to configure us (STA or AP)",
"       The arg 'pin' with no value means to invent a PIN. ",
"       If no pin is given, push button mode is used. ",
"       If 'upnp' is given, configuration via UPnP is enabled. ",
"       Note that this removes any previous 'network' configuration ",
"       The AP or STA to config from is determined heuristically as ",
"       constrained by specified bssid and/or ssid. ",
"       This is the same as the raw CONFIGME command except that it waits ",
"       until done. ",
"    configthem [pin[=<pin>]] [upnp]  [bssid=<macaddr>] [ssid={<text>|0x<hex>}] ",
"               -- use WPS to configure other device ",
"       where device is a station if we are an AP and viceversa. ",
"       The arg 'pin' with no value means to invent a PIN. ",
"       If no pin is given, push button mode is used. ",
"       If 'upnp' is given, configuration via UPnP is enabled. ",
"       The parameters given to the other side come from the current ",
"       network configuration. ",
"       The AP or STA to configure is determined heuristically as ",
"       constrained by specified bssid and/or ssid. ",
"       This is the same as the raw CONFIGTHEM command except that it waits ",
"       until done. ",
"   configstop -- cancel any ongoing WPS operation ",
"   pin  -- print a randomly generated PIN ",
" ",
"NOTES: ",
"Currently, upnp is always enabled for APs (if so compiled) ",
(const char *)0      /* terminator */
};

#include "includes.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <dirent.h>
#include <poll.h>

#include "wpa_ctrl.h"
#include "common.h"
/* maybe: #include "version.h" */


#if 0 /* maybe */
static const char *wpatalk_version =
"wpatalk v" VERSION_STR "\n"
"Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi> and contributors\n"
"Copyright (c) 2008, Atheros Communications\n";
#endif


#if 0	// WAS
static const char *wpatalk_license =
"This program is free software. You can distribute it and/or modify it\n"
"under the terms of the GNU General Public License version 2.\n"
"\n"
"Alternatively, this software may be distributed under the terms of the\n"
"BSD license. See README and COPYING for more details.\n";

static const char *wpatalk_full_license =
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License version 2 as\n"
"published by the Free Software Foundation.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software\n"
"Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA\n"
"\n"
"Alternatively, this software may be distributed under the terms of the\n"
"BSD license.\n"
"\n"
"Redistribution and use in source and binary forms, with or without\n"
"modification, are permitted provided that the following conditions are\n"
"met:\n"
"\n"
"1. Redistributions of source code must retain the above copyright\n"
"   notice, this list of conditions and the following disclaimer.\n"
"\n"
"2. Redistributions in binary form must reproduce the above copyright\n"
"   notice, this list of conditions and the following disclaimer in the\n"
"   documentation and/or other materials provided with the distribution.\n"
"\n"
"3. Neither the name(s) of the above-listed copyright holder(s) nor the\n"
"   names of its contributors may be used to endorse or promote products\n"
"   derived from this software without specific prior written permission.\n"
"\n"
"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
"\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
"LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
"A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
"OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
"SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
"LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
"DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
"THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
"OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
"\n";
#endif

#ifndef PATH_MAX
#define PATH_MAX 256    /* maximum file path length incl termination */
#endif  /* PATH_MAX */

#define COMMAND_MAX 256 /* maximum command line len incl termination */

static const char *progname = "wpatalk";            /* e.g. wpatalk */
static int verbose = 1;
static int interactive = 0;
static int non_interactive = 0;
static int keep_going = 0;
static int warning_displayed = 0;
static const char *ctrl_if_spec;        /* from argv[1] */
static const char *ctrl_ifname;      /* e.g. ath0 */
static char daemon_name[80];      /* wpa_supplicant or hostapd */
static int is_sta;            /* nonzero if talking to wpa_supplicant */
static int is_ap;             /* nonzero if talking to hostapd */
static char run_dir[PATH_MAX] = "/var/run";             /* e.g. /var/run */
static char ctrl_iface_dir[PATH_MAX];      /* e.g. /var/run/<daemon> */
static char socket_path[PATH_MAX]; /* e.g. /var/run/<daemon>/<interface> */
static struct wpa_ctrl *ctrl_conn;      /* open socket file */

static int wpatalk_nerrors = 0;
static int wpatalk_attached = 0;
static int wpatalk_connected = 0;
static int wpatalk_last_id = 0;
static int wpatalk_wps_pending = 0;
static int wpatalk_configme = 0;
static int wpatalk_configthem = 0;

/*=============================================================*/
/*================== FORWARD REFERENCES =======================*/
/*=============================================================*/
static void wpatalk_wps_finish(int success, const char *msg);


/*=============================================================*/
/*=================== STATUS AND ERROR MESSAGES ===============*/
/*=============================================================*/
static void wpatalk_fatal(const char *msg, ...)
{
    va_list ap;
    fprintf(stdout, "%s: FATAL: ", progname);
    va_start(ap, msg);
    vfprintf(stdout, msg, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fprintf(stdout, "(Use -h for help)\n");
    fflush(stdout);
    exit(1);
    return;
}

static void wpatalk_error(const char *msg, ...)
{
    va_list ap;
    fprintf(stdout, "%s: ERROR: ", progname);
    va_start(ap, msg);
    vfprintf(stdout, msg, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    wpatalk_nerrors++;
    return;
}

static void wpatalk_warning(const char *msg, ...)
{
    va_list ap;
    fprintf(stdout, "%s: WARNING: ", progname);
    va_start(ap, msg);
    vfprintf(stdout, msg, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    return;
}

static void wpatalk_info(const char *msg, ...)
{
    va_list ap;
    if (! verbose) return;
    fprintf(stdout, "%s: INFO: ", progname);
    va_start(ap, msg);
    vfprintf(stdout, msg, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    return;
}

static void print_help(void)
{
    int iline;
    for (iline = 0; usage[iline]; iline++) {
        printf("%s\n", usage[iline]);
    }
    return;
}


/*=============================================================*/
/*=============== CONNECTION ==================================*/
/*=============================================================*/

/*static*/ void wpatalk_read_environment(void)
{
    char *alt_run;
    alt_run = getenv("WPA_CTRL_DIR");
    if (alt_run) strncpy(run_dir, alt_run, sizeof(run_dir)-1);
    if (run_dir[0] != '/') wpatalk_fatal("Invalid WPA_CTRL_DIR");
}


/* returns zero if got path ok */
/*static*/ int wpatalk_get_socket_path(void)
{
    char *ep;
    strcpy(daemon_name, "(not determined)");
    strcpy(socket_path, "(not determined)");
    strcpy(ctrl_iface_dir, "(not determined)");
    if (strchr(ctrl_if_spec, '/')) {
        if (socket_path[0] == '/') return 0;     /* once is enough */
        if (ctrl_if_spec[0] == '/') {
            /* absolute path */
            strncpy(socket_path, ctrl_if_spec, sizeof(socket_path)-1);
        } else {
            /* e.g. hostapd/ath0 */
            snprintf(socket_path, sizeof(socket_path),
                "%s/%s", run_dir, ctrl_if_spec);
        }
        ctrl_ifname = strrchr(socket_path, '/') + 1;
        strncpy(ctrl_iface_dir, socket_path, sizeof(ctrl_iface_dir)-1);
        ep = ctrl_iface_dir + strlen(ctrl_iface_dir) - 1;
        if (*ep == '/' ) wpatalk_fatal("Invalid ctrl if spec: %s", ctrl_if_spec);
        while (ep > ctrl_iface_dir && *ep != '/') *ep-- = 0;
        while (ep > ctrl_iface_dir && *ep == '/') *ep-- = 0;
        while (ep > ctrl_iface_dir && *ep != '/') ep--;
        ep++;
        if (!strcmp(ep, "hostapd")) {
            strcpy(daemon_name, "hostapd");
            is_ap = 1;
        } else
        if (!strcmp(ep, "wpa_supplicant")) {
            strcpy(daemon_name, "wpa_supplicant");
            is_sta = 1;
        } else {
            wpatalk_fatal("Cannot determine daemon name from ctrl if spec: %s",
                ctrl_if_spec);
            return 1;
        }
        return 0;
    } else {
        char hpath[PATH_MAX];
        int hgot = 0;
        char wpath[PATH_MAX];
        int wgot = 0;
        ctrl_ifname = ctrl_if_spec;
        snprintf(hpath, sizeof(hpath),
            "%s/%s/%s", run_dir, "hostapd", ctrl_ifname);
        snprintf(wpath, sizeof(wpath),
            "%s/%s/%s", run_dir, "wpa_supplicant", ctrl_ifname);
        hgot = (access(hpath, R_OK|W_OK) == 0);
        wgot = (access(wpath, R_OK|W_OK) == 0);
        if (hgot && wgot) {
            wpatalk_fatal("Conflict. For %s, found both %s and %s", 
                ctrl_ifname, hpath, wpath);
        }
        if (hgot) {
            strcpy(daemon_name, "hostapd");
            strcpy(socket_path, hpath);
            is_ap = 1;
        } else if (wgot) {
            strcpy(daemon_name, "wpa_supplicant");
            strcpy(socket_path, wpath);
            is_sta = 1;
        } else {
            wpatalk_warning("No daemon for interface %s ? "
                "Did not find either %s or %s", ctrl_ifname, hpath, wpath);
            return 1;
        }
        snprintf(ctrl_iface_dir, sizeof(ctrl_iface_dir), "%s/%s",
            run_dir, daemon_name);
        return 0;
    }
}


static void wpatalk_close_connection(void)
{
	if (ctrl_conn == NULL)
		return;

	if (wpatalk_attached) {
		wpa_ctrl_detach(ctrl_conn);
		wpatalk_attached = 0;
	}
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
}


/* wpatalk_reconnect -- maybe reconnect.
 * If previously connected, disconnects and aborts unless "keep_going"
 * is set (-k option).
 */
static void wpatalk_reconnect(void)
{
    if (ctrl_conn) {
        wpatalk_close_connection();
        if (!keep_going) {
            wpatalk_fatal("Exiting due to lost connection");
        }
    }
    for (;;) {
        if (wpatalk_get_socket_path() == 0) {
            #ifdef CONFIG_CTRL_IFACE_UNIX
            ctrl_conn = wpa_ctrl_open(socket_path);
            #else
            #error "wpatalk_open_connection configuration error"
            #endif
        }
        if (ctrl_conn) {
            if (wpa_ctrl_attach(ctrl_conn) == 0) {
                if (warning_displayed || verbose) {
                    wpatalk_info(
                        "Connection (re)established to daemon=%s interface=%s",
                        daemon_name, ctrl_ifname);
                    wpatalk_info("... using socket-file=%s", socket_path);
                    warning_displayed = 0;
 	        }
                wpatalk_attached = 1;
                return;
            } else {
                if (!warning_displayed) {
                    wpatalk_warning(
                        "Failed to attach to daemon %s", daemon_name);
                }
                wpatalk_close_connection();
                ctrl_conn = NULL;
            }
        }
        if (!keep_going) {
            wpatalk_fatal("Failed to connect to daemon %s errno %d "
                        "using socket file %s", 
                daemon_name, errno, socket_path);
            return;
        }
        if (!warning_displayed) {
            wpatalk_info("Could not connect to daemon %s -- re-trying",
                    daemon_name);
            warning_displayed = 1;
        }
        os_sleep(1, 0);
    }

    return;
}



/*=============================================================*/
/*================= HANDSHAKING WITH DAEMON ===================*/
/*=============================================================*/


static void wpatalk_action_process(const char *msg);

static void wpatalk_msg_cb(char *msg, size_t len)
{
	wpatalk_info("GOT(cb): %s", msg);
        msg[len] = 0;   /* to be sure */
        wpatalk_action_process(msg);
}


static int wpa_ctrl_command(struct wpa_ctrl *ctrl_conn, const char *cmd)
{
	char buf[2048];
	size_t len;
	int ret;

	if (verbose) {
		wpatalk_info("SEND-RAW: %s", cmd);
	}
	if (ctrl_conn == NULL) {
		wpatalk_error("Not connected to daemon - command dropped.");
		return -1;
	}
	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl_conn, cmd, os_strlen(cmd), buf, &len,
			       wpatalk_msg_cb);
	if (ret == -2) {
		wpatalk_error("'%s' command timed out.", cmd);
		return -2;
	} else if (ret < 0) {
		wpatalk_error("'%s' command failed.", cmd);
		return -1;
	}
      	buf[len] = '\0';
        while (len > 0 && !isgraph(buf[len-1])) buf[--len] = '\0';
        if (verbose) 
	    wpatalk_info("GOT-RESPONSE: %s", buf);
        else
            printf("%s\n", buf);
        if (!strcmp(buf, "UNKNOWN COMMAND")) {
            wpatalk_error("Unknown command: %s", cmd);
            return -1;
        }
        if (!strcmp(buf, "FAIL")) {
            wpatalk_error("Failed command: %s", cmd);
            return -1;
        }
	return 0;
}


static int str_match(const char *a, const char *b)
{
    return strncmp(a, b, strlen(b)) == 0;
}

static void wpatalk_action_process(const char *msg)
{
	const char *pos;
	char *copy = NULL, *id, *pos2;

	pos = msg;
	if (*pos == '<') {
		/* skip priority */
		pos = os_strchr(pos, '>');
		if (pos)
			pos++;
		else
			pos = msg;
	}

	if (str_match(pos, WPA_EVENT_CONNECTED)) {
		int new_id = -1;
		os_unsetenv("WPA_ID");
		os_unsetenv("WPA_ID_STR");
		os_unsetenv("WPA_CTRL_DIR");

		pos = os_strstr(pos, "[id=");
		if (pos)
			copy = os_strdup(pos + 4);

		if (copy) {
			pos2 = id = copy;
			while (*pos2 && *pos2 != ' ')
				pos2++;
			*pos2++ = '\0';
			new_id = atoi(id);
			os_setenv("WPA_ID", id, 1);
			while (*pos2 && *pos2 != '=')
				pos2++;
			if (*pos2 == '=')
				pos2++;
			id = pos2;
			while (*pos2 && *pos2 != ']')
				pos2++;
			*pos2 = '\0';
			os_setenv("WPA_ID_STR", id, 1);
			os_free(copy);
		}

		os_setenv("WPA_CTRL_DIR", ctrl_iface_dir, 1);

		if (!wpatalk_connected || new_id != wpatalk_last_id) {
			wpatalk_connected = 1;
			wpatalk_last_id = new_id;
		}
                return;
	} 
        if (str_match(pos, WPA_EVENT_DISCONNECTED)) {
		if (wpatalk_connected) {
			wpatalk_connected = 0;
		}
                return;
	} if (str_match(pos, WPA_EVENT_TERMINATING)) {
		wpatalk_error("daemon is terminating");
                os_sleep(2,0);
		wpatalk_reconnect();
                return;
	}
        
        /* WPS-related messages.
         * All WPS "jobs" send CTRL-REQ-WPS_JOB_DONE when done.
         * They first send CTRL-REQ-EAP-WPS-SUCCESS if successful.
         */
        if (str_match(pos, "CTRL-REQ-EAP-WPS-SUCCESS")) {
                if (wpatalk_wps_pending) {
                    wpatalk_wps_finish(1, pos);
                }
                return;
	} 
        if (str_match(pos, "CTRL-REQ-WPS-JOB-DONE")) {
                /* If we didn't stop due to success, must be failure */
                if (wpatalk_wps_pending) {
                    wpatalk_wps_finish(0, pos);
                }
                return;
	} 
}


static void wpatalk_action_cb(char *msg, size_t len)
{
	wpatalk_action_process(msg);
}


/* wpatalk_recv_pending -- process pending packets from authentication
 * daemon, and return > 0 if there are more packets (less than zero
 * if error).
 * If block is set to 1, will sleep waiting for up to one packet.
 */
static int wpatalk_recv_pending(int block)
{
        struct pollfd fds[1];
        int nfds;
        int more = 0;
        int timeout_msec = 0;    /* negative for infinity */
        char buf[512]; /* note: large enough to fit in unsolicited messages */
	size_t len;
        int nloops;

        for (nloops = 0; ; nloops++) {
	    if (ctrl_conn == NULL) {
		return -1;
	    }
            fds[0].fd = wpa_ctrl_get_fd(ctrl_conn);
            fds[0].events = POLLIN;
            fds[0].revents = 0;
            nfds = 1;
            if (nloops > 0) timeout_msec = 1000;
            else timeout_msec = 0;
            nfds = poll(fds, nfds, timeout_msec);
            if (nfds < 0) {
                if (errno == EINTR) continue;
                #if 1
		wpatalk_error("Connection to daemon lost");
                #else
                wpatalk_fatal("Died on poll() error %d", errno);
                #endif
                return -1;
            }
            more = (nfds > 0);
            if (more == 0) {
                if (block == 0) {
                    return 0;       /* nothing pending */
                } else {
                    /* blocking mode */
                    /* verify that connection is still working */
                    size_t len = sizeof(buf) - 1;
                    if (wpa_ctrl_request(ctrl_conn, "PING", 4, buf, &len,
	    		        wpatalk_action_cb) < 0 ||
	                len < 4 || os_memcmp(buf, "PONG", 4) != 0) {
	    	        wpatalk_error("daemon did not reply to PING");
                        return -1;
	            }
                    /* and try again */
                    continue;
                }
            }
	    len = sizeof(buf) - 1;
	    if (wpa_ctrl_recv(ctrl_conn, buf, &len) == 0) {
	    	buf[len] = '\0';
	        wpatalk_info("GOT: %s", buf);
	    	wpatalk_action_process(buf);
	    } else {
	    	wpatalk_error("Could not read pending message.");
	    }
            break;
	}
        return more;
}


/*=============================================================*/
/*================ COMMAND PROCESSING =========================*/
/*=============================================================*/


/* isword -- are we at a word?
 */
int isword(const char *cmd)
{
    return (isgraph(*cmd) && *cmd != '=');
}


#if 0   /* maybe */
/* wordlen -- return length of word
 *      where words are sequences of graphical chars
 */
static int wordlen(const char *word) 
{
    const char *startword = word;
    while (isgraph(*word) && *word != '=') word++;
    return word - startword;
}
#endif

/* wordeq -- return nonzero if first word in word1 == first word in word2
 *      where words are sequences of graphical chars
 */
static int wordeq(const char *word1, const char *word2)
{
    while (isgraph(*word1) && isgraph(*word2) && *word1 == *word2){
        word1++, word2++;
    }
    if (isgraph(*word1) || isgraph(*word2)) return 0;
    return 1;   /* match! */
}

#if 0   /* maybe */
/* tageq -- return nonzero if first tag in word1 == first tag in word2
 *      where tags are sequences of graphical chars excluding '=' 
 */
static int tageq(const char *word1, const char *word2)
{
    while (isgraph(*word1) && *word1 != '=' &&
            isgraph(*word2) && *word2 != '=' &&
            *word1 == *word2 ) {
        word1++, word2++;
    }
    if ((*word1 == '=' || !isgraph(*word1)) &&
        (*word2 == '=' || !isgraph(*word2))) return 1;   /* match */
    return 0;   /* no match */
}
#endif

#if 0   /* maybe */
/* tagvalue -- return pointer to value part of word with tag=value
 * If no equal sign, returns pointer to past end of word;
 * thus tag with no equal and tag= with no value are the same.
 */
static const char *tagvalue(const char *word) 
{
    while (isgraph(*word) && *word != '=') word++;
    if (*word == '=') return word+1;
    return word;
}
#endif

#if 0   /* maybe */
/* wordnext -- skip current word, return ptr to next word
 */
static const char *wordnext(const char *cmd)
{
    while (isgraph(*cmd)) cmd++;                /* skip word */
    while (*cmd && !isgraph(*cmd)) cmd++;       /* skip whitespace */
    return cmd;
}
#endif

/* Finish up WPS configuration operation: we are AP.
 **/
static void wpatalk_configme_finish_ap(int success, const char *msg)
{
    wpatalk_wps_pending = 0;
    if (!success) {
        wpatalk_error("WPS Failure");
        return;
    }
    return;
}


/* Finish up WPS configuration operation: we are station.
 **/
static void wpatalk_configme_finish_sta(int success)
{
    wpatalk_wps_pending = 0;

    if (!success) {
        wpatalk_error("WPS failed!");
        return;
    }

    wpatalk_info("AP configured us OK! We should be connecting now!");
    return;
}

/* Finish up WPS configuration operation: we are AP.
 **/
static void wpatalk_configthem_finish_ap(int success)
{
    wpatalk_wps_pending = 0;

    if (!success) {
        wpatalk_error("WPS operation on AP failed");
        return;
    }

    wpatalk_info("DONE: Station configured OK by us!");
    return;
}

/* Finish up WPS configuration operation: we are station.
 **/
static void wpatalk_configthem_finish_sta(int success)
{
    wpatalk_wps_pending = 0;

    if (!success) {
        wpatalk_error("WPS operation on STA failed");
        return;
    }

    wpatalk_info("DONE: AP configured OK by us!");
    return;
}


static void wpatalk_wps_finish(int success, const char *msg)
{
    if (wpatalk_configme) {
        if (is_ap) wpatalk_configme_finish_ap(success, msg);
        else
        if (is_sta) wpatalk_configme_finish_sta(success);
        else
        wpatalk_error("wpatalk_wps_finish: don't know if ap or sta");
    } else
    if (wpatalk_configthem) {
        if (is_ap) wpatalk_configthem_finish_ap(success);
        else
        if (is_sta) wpatalk_configthem_finish_sta(success);
        else
        wpatalk_error("wpatalk_wps_finish: don't know if ap or sta");
    }
}

/* pin_gen -- generate an 8 character PIN number.
 */
static void pin_gen(char pwd[9])
{
        unsigned long pin;
        unsigned char checksum;
        unsigned long acc = 0;
        unsigned long tmp;

        /* use random numbers between [10000000, 99999990)
        * so that first digit is never a zero, which could be confusing.
        */
        pin = 1000000 + rand() % 9000000;
        tmp = pin * 10;

        acc += 3 * ((tmp / 10000000) % 10);
        acc += 1 * ((tmp / 1000000) % 10);
        acc += 3 * ((tmp / 100000) % 10);
        acc += 1 * ((tmp / 10000) % 10);
        acc += 3 * ((tmp / 1000) % 10);
        acc += 1 * ((tmp / 100) % 10);
        acc += 3 * ((tmp / 10) % 10);

        checksum = (unsigned char)(10 - (acc % 10)) % 10;
        snprintf(pwd, 9, "%08lu", pin * 10 + checksum);
}


/* configme_sta -- builtin command to configure "us" when we are a station.
 */
static void configme_sta(const char *cmd_in)
{
    char *rawcmd;

    if (! is_sta) {
        wpatalk_error("This command valid for STA only.");
        return;
    }

    /* Rename cmd to it's upper case equivalent, and pass all to
     * wpa_supplicant.
     */
    rawcmd = strdup(cmd_in);
    memcpy(rawcmd, "CONFIGME", 8);
    if (wpa_ctrl_command(ctrl_conn, rawcmd)) {
            wpatalk_error("Raw command failed: %s", rawcmd);
            free(rawcmd);
            return;
    }

    wpatalk_wps_pending = 1;
    wpatalk_configme = 1;
    wpatalk_configthem = 0;

    if (interactive) {
        wpatalk_info("--> WAIT FOR WPS TO BE DONE <--");
        return;
    } 
    wpatalk_info("Waiting for WPS TO BE DONE");
    while (wpatalk_wps_pending) {
        if (wpatalk_recv_pending(1/*block*/) < 0) {
            wpatalk_error("Wait for WPS aborted due to error");
            break;
        }
    }
    return;
}


/* configme_ap -- builtin command to configure "us" when we are an AP
 */
static void configme_ap(const char *cmd_in)
{
    char *rawcmd;

    if (! is_ap) {
        wpatalk_error("This command valid for AP only.");
        return;
    }

    /* Rename cmd to it's upper case equivalent, and pass all to
     * hostapd.
     */
    rawcmd = strdup(cmd_in);
    memcpy(rawcmd, "CONFIGME", 8);
    if (wpa_ctrl_command(ctrl_conn, rawcmd)) {
            wpatalk_error("Raw command failed: %s", rawcmd);
            free(rawcmd);
            return;
    }

    wpatalk_wps_pending = 1;
    wpatalk_configme = 1;
    wpatalk_configthem = 0;

    if (interactive) {
        wpatalk_info("--> WAIT FOR WPS TO BE DONE <--");
        return;
    } 
    wpatalk_info("Waiting for WPS TO BE DONE");
    while (wpatalk_wps_pending) {
        if (wpatalk_recv_pending(1/*block*/) < 0) {
            wpatalk_error("Wait for WPS aborted due to error");
            break;
        }
    }
    return;
}

/* configthem_sta -- builtin command to configure "them" when we are station
 * This requires (in addition to optional pin=<pin> tagged value)
 * a network id to configure from (nwid=<decimal>).
 */
static void configthem_sta(const char *cmd_in)
{
    char *rawcmd;

    if (! is_sta) {
        wpatalk_error("This command valid for STA only.");
        return;
    }

    /* Rename cmd to it's upper case equivalent, and pass all to
     * wpa_supplicant.
     */
    rawcmd = strdup(cmd_in);
    memcpy(rawcmd, "CONFIGTHEM", 10);
    if (wpa_ctrl_command(ctrl_conn, rawcmd)) {
            wpatalk_error("Raw command failed: %s", rawcmd);
            free(rawcmd);
            return;
    }

    wpatalk_wps_pending = 1;
    wpatalk_configme = 0;
    wpatalk_configthem = 1;

    if (interactive) {
        wpatalk_info("--> WAIT FOR WPS TO BE DONE <--");
        return;
    } 

    wpatalk_info("Waiting for WPS TO BE DONE");
    while (wpatalk_wps_pending) {
        if (wpatalk_recv_pending(1/*block*/) < 0) {
            wpatalk_error("Wait for WPS aborted due to error");
            break;
        }
    }
    return;
}


/* configthem_ap -- builtin command to configure "them" when we are an AP.
 */
static void configthem_ap(const char *cmd_in)
{
    char *rawcmd;

    if (! is_ap) {
        wpatalk_error("This command valid for AP only.");
        return;
    }

    /* Rename cmd to it's upper case equivalent, and pass all to
     * wpa_supplicant.
     */
    rawcmd = strdup(cmd_in);
    memcpy(rawcmd, "CONFIGTHEM", 10);
    if (wpa_ctrl_command(ctrl_conn, rawcmd)) {
            wpatalk_error("Raw command failed: %s", rawcmd);
            free(rawcmd);
            return;
    }

    wpatalk_wps_pending = 1;
    wpatalk_configme = 0;
    wpatalk_configthem = 1;

    if (interactive) {
        wpatalk_info("--> WAIT FOR WPS TO BE DONE <--");
        return;
    } 

    wpatalk_info("Waiting for WPS TO BE DONE");
    while (wpatalk_wps_pending) {
        if (wpatalk_recv_pending(1/*block*/) < 0) {
            wpatalk_error("Wait for WPS aborted due to error");
            break;
        }
    }
    return;
}

static void wps_config(char** argv)
{
    char argbuf[256];
    char ** arg;
    int pos;
    const char* cmd;
  
    cmd=*argv;
    pos=sprintf(argbuf,cmd);
    arg=argv+1;

    while (*arg != NULL)
    {
	argbuf[pos++]=' ';
	pos+=sprintf(argbuf+pos,*arg);
	arg++;
    }
	
//    wpatalk_error("WPS Do cmd  <%s>",argbuf);
    if (wordeq(cmd, "configme")) {
        if (is_sta) configme_sta(argbuf);
        else
        if (is_ap) configme_ap(argbuf);
        else
        wpatalk_error("Don't know if we're a STA or an AP");
        return;
    }
    if (wordeq(cmd, "configthem")) {
        if (is_sta) configthem_sta(argbuf);
        else
        if (is_ap) configthem_ap(argbuf);
        else
        wpatalk_error("Don't know if we're a STA or an AP");
        return;
    }

    if (wordeq(cmd, "pin")) {
        char pin[9];
        pin_gen(pin);
        printf("RANDOMLY GENERATED PIN == %s\n", pin);
        fflush(stdout);
        return;
    }	

    if (interactive) 
        wpatalk_error("Command not found: %s", cmd);
    else
        wpatalk_fatal("Command not found: %s", cmd);
    return;

}

/*
 * Note: cmd must be cleaned up before being passed for execution
 */
static void wpatalk_command_execute(const char *cmd)
{
    if (wordeq(cmd, "configstop")) {
        cmd = "CONFIGSTOP";
    }
    if (isupper(*cmd)) {
        if (wpa_ctrl_command(ctrl_conn, cmd)) {
            if (interactive || keep_going) 
                wpatalk_error("Command failed: %s", cmd);
            else
                wpatalk_fatal("Command failed: %s", cmd);
        }
        return;
    }
#if 0	
    if (wordeq(cmd, "configme")) {
        if (is_sta) configme_sta(cmd);
        else
        if (is_ap) configme_ap(cmd);
        else
        wpatalk_error("Don't know if we're a STA or an AP");
        return;
    }
    if (wordeq(cmd, "configthem")) {
        if (is_sta) configthem_sta(cmd);
        else
        if (is_ap) configthem_ap(cmd);
        else
        wpatalk_error("Don't know if we're a STA or an AP");
        return;
    }
    if (wordeq(cmd, "pin")) {
        char pin[9];
        pin_gen(pin);
        printf("RANDOMLY GENERATED PIN == %s\n", pin);
        fflush(stdout);
        return;
    }
#endif	
    if (interactive) 
        wpatalk_error("Command not found: %s", cmd);
    else
        wpatalk_fatal("Command not found: %s", cmd);
    return;
}

/*=============================================================*/
/*=================== STDIN HANDLING ==========================*/
/*=============================================================*/

/* We can't reliably use fgets in a poll'd situation, thus the following.
 * Unix provides no sure way to read exactly one line at a time at
 * the system call level.
 * fgets addresses this but wants to block for additional input
 * if there if a line is not easily read, and gets upset if the
 * file descriptor has been placed in a non-blocking mode.
 * There is also the problem that fgets may have more than
 * one line buffered up, without a way to determine this.
 */

static char stdin_buf[1024];
int stdin_buf_count;

/* read_stdin reads from stdin and buffers that.
 * Call only when poll() says it is ready.
 */
void read_stdin(void)
{
    int nread;

    nread = read(0/*STDIN*/, stdin_buf+stdin_buf_count, 
        sizeof(stdin_buf)-stdin_buf_count);
    if (nread <= 0) {
        wpatalk_fatal("EOF on stdin");
        return;
    }
    stdin_buf_count += nread;
    return;
}

/* extract_stdin_line -- get buffered text originally from stdin.
 * returns zero if line was successfully extracted from
 * buffer, or nonzero otherwise.
 */
static int extract_stdin_line(char *buf, int buf_size)
{
    int idx;
    int linelen;

    /* Do we have a line already? */
    for (idx = 0; idx < stdin_buf_count; idx++) {
        if (stdin_buf[idx] == '\n') {
            linelen = idx+1;
            goto found_line;
        }
    }
    if (idx >= sizeof(stdin_buf)) {
        wpatalk_fatal("Overlong input line");
    }
    return 1;

    found_line:
    if (linelen >= buf_size)  {
        wpatalk_fatal("Overlong input line");
        return 1;
    }
    memcpy(buf, stdin_buf, linelen);
    buf[linelen] = 0;   /* null termination */
    if (linelen >= stdin_buf_count) {
        stdin_buf_count = 0;
    } else {
        memmove(stdin_buf, stdin_buf+linelen, stdin_buf_count - linelen);
        stdin_buf_count -= linelen;
    }
    return 0;
}


/* process a command read from a file (or stdin) */
static void command_from_file(char *buf)
{
    char *cmd = buf;
    char *ep;
    /* clean leading/ trailing whitespace */
    while (*cmd && !isgraph(*cmd)) cmd++;
    ep = cmd + strlen(cmd);
    while (ep > cmd && !isgraph(*--ep)) *ep = 0;
    /* skip empty lines and comment lines */
    if (*cmd == 0 || *cmd == '#') return;
    wpatalk_info("From stdin: %s", cmd);
    wpatalk_command_execute(cmd);
    return;
}

void interactive_from_stdin(void)
{
    for (;;)
    {
        const int timeout_msec = 1000;    /* negative for infinity */
        struct pollfd fds[2];
        int nfds;

        fds[0].fd = wpa_ctrl_get_fd(ctrl_conn);
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = 0;  /* STDIN */
        fds[1].events = POLLIN;
        fds[1].revents = 0;
        nfds = 2;
        nfds = poll(fds, nfds, timeout_msec);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            wpatalk_fatal("Died on poll() error %d", errno);
            continue;
        }
        if (nfds == 0) {
            /* timeout */
	    char buf[512]; /* note: large enough to fit in unsolicited messages */
	    /* verify that connection is still working */
	    size_t len = sizeof(buf) - 1;
	    if (wpa_ctrl_request(ctrl_conn, "PING", 4, buf, &len,
	    		     wpatalk_action_cb) < 0 ||
	        len < 4 || os_memcmp(buf, "PONG", 4) != 0) {
	    	wpatalk_error("daemon did not reply to PING command");
                goto reconnect;
	    }
            continue;
        }
        if ((fds[0].revents & POLLIN) != 0) {
            for (;;) {
                int status = wpatalk_recv_pending(0/*block*/);
                if (status > 0) continue;       /* more */
                if (status < 0) {
                    wpatalk_warning("Closing connection due to error.");
                    goto reconnect;
                }
                break;
            }
        }
        if ((fds[1].revents & POLLIN) != 0) {
            char buf[512];
            /* only one read() per ready to avoid blocking */
            read_stdin();
            /* the one read() may result in 0, 1 or more input lines */
            while (extract_stdin_line(buf, sizeof(buf)) == 0) {
                command_from_file(buf);
            }
        }

        continue;

        reconnect:
        wpatalk_reconnect();
        continue;
    }
}

void noninteractive_from_stdin(void)
{
    for (;;)
    {
        char buf[512];
        char *cmd = buf;
        char *ep;
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            wpatalk_info("EOF");
            break;
        }
        /* clean leading/ trailing whitespace */
        while (*cmd && !isgraph(*cmd)) cmd++;
        ep = cmd + strlen(cmd);
        while (ep > cmd && !isgraph(*--ep)) *ep = 0;
        /* skip empty lines and comment lines */
        if (*cmd == 0 || *cmd == '#') continue;
        wpatalk_command_execute(cmd);
        if (wpatalk_nerrors && !keep_going) {
            wpatalk_fatal("Exiting due to error(s)");
        }
    }
}

/*=============================================================*/
/*=================== MAIN ====================================*/
/*=============================================================*/

static void wpatalk_cleanup(void)
{
	wpatalk_close_connection();
}

static void wpatalk_terminate(int sig)
{
	wpatalk_cleanup();
	exit(0);
}


int main(int argc, char *argv[])
{
    const char *arg;

    progname = *argv++;
    if (strrchr(progname,'/') ) progname = strrchr(progname,'/')+1;

    srand(time(0));     /* get random numbers */
    wpatalk_read_environment();

    /* Options */
    while ((arg = *argv++) != NULL) {
        if (*arg != '-') break;
        if (!strcmp(arg,"-h") || 
                !strcmp(arg,"-help") || 
                !strcmp(arg,"--help")) {
            print_help();
            exit(0);
            continue;
        }
        if (!strcmp(arg, "-v")) {
            verbose++;
            continue;
        }
        if (!strcmp(arg, "-q")) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (!strcmp(arg, "-i")) {
            interactive = 1;
            non_interactive = 0;
            continue;
        }
        if (!strcmp(arg, "-noi")) {
            non_interactive = 1;
            interactive = 0;
            continue;
        }
        if (!strcmp(arg, "-k")) {
            keep_going = 1;
            continue;
        }
        wpatalk_fatal("Unknown arg: %s", arg);
    }
    if (!arg || ! *arg) {
        wpatalk_fatal("Missing interface name");
    }
    ctrl_if_spec = arg;
    if (*argv) {
        /* default, non-interactive, stop on first error */
    } else {
        /* No args, read stdin, default to interactive mode for terminals */
        if (!interactive && !non_interactive) interactive = isatty(0);
    }

    /* Open interface */
    atexit(wpatalk_cleanup);
    signal(SIGHUP, wpatalk_terminate);
    signal(SIGINT, wpatalk_terminate);
    signal(SIGTERM, wpatalk_terminate);
    wpatalk_reconnect();

    /* Commands */
    if (! *argv) {
        /* No args ... read stdin */
        if (interactive) {
            interactive_from_stdin();
        } else {
            noninteractive_from_stdin();
        }
    } else {
        const char *cmd;

        cmd=*argv;
        if (wordeq(cmd, "configthem") ||
		wordeq(cmd, "configme") ||
		wordeq(cmd, "pin")) {
	 	wps_config(argv);
		return 0;
        }			
		
        while ((cmd = *argv++) != NULL) {
            wpatalk_command_execute(cmd);
            if ((! keep_going) && wpatalk_nerrors) {
                wpatalk_fatal("Exiting due to error(s).");
            }
        }
    }
    if (wpatalk_nerrors) {
        wpatalk_fatal("There were error(s)");
    }
    return 0;
}


/*=============================================================*/
/*=================== END =====================================*/
/*=============================================================*/


