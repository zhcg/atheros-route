/* apstart -- set up network interface per topology file.
*/


#include "common.h"
#include "topology.c"

char *help[] = {
"apstart -- bring up (or down) network interfaces per topology file. ",
" ",
"Usage:  apstart [<option>]... <topology file>  ",
" ",
"Options: ",
"    -h | -help | --help    -- print this help message and exit ",
"    -stop                  -- bring down network interfaces ",
"    -dryrun                -- show what would be done but don't do it",
"  ",
};


int main(int argc, char **argv)
{
        char *filepath = NULL;
        int bringup = 1;
        int dry_run_flag = 0;
        char *arg;

        argv++; /* skip program name */

        while ((arg = *argv++) != NULL) {
                if (!strcmp(arg, "-h") || 
                                !strcmp(arg, "-help") ||
                                !strcmp(arg, "--help")) {
                        int iline;
                        for (iline = 0; iline < sizeof(help)/sizeof(help[0]);
                                        iline++) {
                                printf("%s\n", help[iline]);
                        }
                        return 0;
                }
                if (!strcmp(arg, "-stop")) {
                        bringup = 0;
                        continue;
                }
                if (!strcmp(arg, "-dryrun")) {
                        dry_run_flag = 1;
                        continue;
                }
                if (*arg != '-') {
                        if (filepath) {
                                wpa_printf(MSG_ERROR, "Too many args");
                                return 1;
                        }
                        filepath = arg;
                        continue;
                }
                wpa_printf(MSG_ERROR, "Unknown arg: %s", arg);
                return 1;
        }

        if (filepath == NULL) {
                wpa_printf(MSG_ERROR, "Need topology file as required arg.");
                return 1;
        }
        wpa_printf(MSG_INFO, "Using topology file %s", filepath);

        if (apstart_read_topology_file(filepath, bringup, dry_run_flag)) {
                wpa_printf(MSG_ERROR, "Failure!");
                return 1;
        }
        return 0;
}
