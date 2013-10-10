#ifndef topology__h
#define topology__h 1

#include "includes.h"
#include "common.h"
#include "wpa_supplicant_i.h"
#include <linux/if.h>                   /* for IFNAMSIZ etc. */

/* wpa_supplicant_config_read_topology_file reads a topology file,
 * which defines radios and virtual stas, each of which have separate
 * config files.
 * Returns nonzero if error.
 */
int wpa_supplicant_config_read_topology_file(
        struct wpa_global *global,
        char *filepath
        );

/* wpa_supplicant_config_read_topology_files reads one or more topology files,
 * which define radios and virtual stas, each of which have separate
 * config files.
 * Returns nonzero if error.
 */
int wpa_supplicant_config_read_topology_files(
        struct wpa_global *global,
        char **filepaths
        );

#endif  /* topology__h */

