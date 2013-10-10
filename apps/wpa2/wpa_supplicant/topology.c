
/*
 * Topology file format:
 * Contains lines of the following lexical types:
 *      empty lines
 *      comments lines:  #...
 *      labelled lines:  keyword ...
 *      section begin:  { ...
 *      section end:    } ...
 * Sections (between section begin and end) belong to previous labelled line.
 * We recognize the following:
 * bridge <name>
 * {
 *      interface <name>
 * }
 * radio <name>
 * {
 *     sta <interface_name>
 *     {
 *         driver <driver_name>  
 *         config <filepath>
 *     }
 * }
 *
 * radio, sta may be repeated within their section.
 * The special bridge name "none" is allowed (to not use a bridge
 * with the interface(s)).
 */

#include "includes.h"
#include "topology.h"

static char *word_first(char *s)
{
        while (*s && !isgraph(*s)) s++;
        return s;
}

static char *word_next(char *s)
{
        while (isgraph(*s)) s++;
        while (*s && !isgraph(*s)) s++;
        return s;
}

static int word_empty(char *s)
{
        return (*s == 0 || *s == '#');
}


static int word_eq(char *s, char *match)
{
        if (!isgraph(*s)) return 0;
        while (isgraph(*s)) {
                if (*s != *match) return 0;
                s++, match++;
        }
        if (isgraph(*match)) return 0;
        return 1;
}

static int word_len(char *s)
{
        int len = 0;
        while (isgraph(*s)) s++, len++;
        return len;
}

/* If it fits, copy word with null termination into out and return 0.
 * Else return 1.
 */
static int word_get(char *s, char *out, int out_size)
{
        int len = word_len(s);
        int i;
        if (len >= out_size) return 1;
        for (i = 0; i < len; i++) out[i] = s[i];
        out[i] = 0;
        return 0;
}


/* Information we carry around while parsing a topology file 
 * and the config files it refers to
 */
struct topology_bridge;
struct topology_iface;
struct topology_bridge  {
        char name[IFNAMSIZ+1];
        /* Sibling bridges (linked list) */
        struct topology_bridge *next;
        struct topology_bridge *prev;
        /* child interfaces */
        struct topology_iface *ifaces;
};
struct topology_iface {
        char name[IFNAMSIZ+1];
        struct topology_bridge *bridge; /* parent */
        /* Sibling interfaces (linked list) */
        struct topology_iface *next;
        struct topology_iface *prev;
        int used;
};
struct topology_parse {
        int errors;
        char *filepath;
        int line;
        FILE *f;
        char buf[256];
        struct topology_bridge *bridges;
        struct wpa_global *global;
        char radio_name[IFNAMSIZ+1];
        char ifname[IFNAMSIZ+1];
        char bridge_name[IFNAMSIZ+1];
        char driver[33];
        char ctrl_interface[256];
	struct wpa_interface *wiface;
        char config_filepath[256];
};


static char * topology_line_get(
        struct topology_parse *p
        )
{
        char *s;
        if (fgets(p->buf, sizeof(p->buf), p->f) == NULL) {
                return NULL;
        }
        /* Make sure we got a full line. Chop off trailing whitespace */
        s = strchr(p->buf,'\r');
        if (s == NULL) s = strchr(p->buf,'\n');
        if (s == NULL) {
                wpa_printf(MSG_ERROR, "File %s line %d is overlong!",
                        p->filepath, p->line);
                p->buf[0] = 0;
        } else {
                *s = 0;
                while (s > p->buf) {
                        int ch = *--s;
                        if (isgraph(ch)) break;
                        else *s = 0;
                }
        }
        p->line++;
        /* Chop off leading whitespace */
        s = word_first(p->buf);
        if (word_empty(s)) *s = 0;      /* chop comments */
        return s;
}

/* Skip lines until we get a closing brace (recursive)
 */
static int topology_skip_section(
        struct topology_parse *p
        )
{
        char *s;
        int depth = 1;
        while ((s = topology_line_get(p)) != NULL) {
               if (*s == '{') depth++;
               else
               if (*s == '}') depth--;
               if (depth <= 0) break;
        }
        if (depth != 0) {
                wpa_printf(MSG_ERROR, 
                        "Topology file %s line %d: unbalanced braces",
                        p->filepath, p->line);
                p->errors++;
                return 1;
        }
        return 0;
}

/* skip lines until we get opening brace... error if non-empty
 * line found between.
 */
static int topology_find_section(
        struct topology_parse *p
        )
{
        char *s;
        int depth = 0;
        while ((s = topology_line_get(p)) != NULL) {
               if (*s == 0) continue;   /* allow empty lines */
               if (*s == '{') { depth++; break; }
               if (*s == '}') { depth--; break; }
               break;
        }
        if (depth <= 0) {
                wpa_printf(MSG_ERROR, 
                        "Topology file %s line %d: missing left brace",
                        p->filepath, p->line);
                p->errors++;
                return 1;
        }
        return 0;
}


static struct topology_bridge *topology_findbridge(
        struct topology_parse *p,
        char *name
        )
{
        struct topology_bridge *bridge = p->bridges;
        struct topology_bridge *first_bridge = bridge;
        if (bridge == NULL) return NULL;
        do {
                if (!strcmp(bridge->name, name)) {
                        return bridge;
                }
                bridge = bridge->next;
        } while (bridge != first_bridge);
        return NULL;
}

static struct topology_iface *topology_find_iface(
        struct topology_parse *p,
        char *name
        )
{
        struct topology_bridge *bridge = p->bridges;
        struct topology_bridge *first_bridge = bridge;
        if (bridge == NULL) return NULL;
        do {
                struct topology_iface *iface = bridge->ifaces;
                struct topology_iface *first_iface = iface;
                if (iface != NULL) 
                do {
                        if (!strcmp(iface->name, name)) {
                                return iface;
                        }
                        iface = iface->next;
                } while (iface != first_iface);
                bridge = bridge->next;
        } while (bridge != first_bridge);
        return NULL;
}

static struct topology_bridge *topology_add_bridge(
        struct topology_parse *p
        )
{
        /* p->bridge_name contains bridge name */
        struct topology_bridge *bridge;
        bridge = topology_findbridge(p, p->bridge_name);
        if (bridge != NULL) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Duplicate bridge %s",
                        p->filepath, p->line, p->bridge_name);
                p->errors++;
                return NULL;
        }

        bridge = os_zalloc(sizeof(*bridge));
        if (bridge == NULL) {
                wpa_printf(MSG_ERROR, "Malloc error!");
                p->errors++;
                return NULL;
        }
        memcpy(bridge->name, p->bridge_name, sizeof(bridge->name));
        if (p->bridges) {
                bridge->next = p->bridges;
                bridge->prev = bridge->next->prev;
                bridge->next->prev = bridge;
                bridge->prev->next = bridge;
        } else {
                bridge->next = bridge->prev = bridge;
        }
        p->bridges = bridge;    /* point to "current" bridge */
        return bridge;
}

static struct topology_iface *topology_add_iface(
        struct topology_parse *p
        )
{
        /* p->ifname contains interface name; p->bridges points to bridge */
        struct topology_bridge *bridge = p->bridges;
        struct topology_iface *iface;
        iface = topology_find_iface(p, p->ifname);
        if (iface != NULL) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Duplicate iface %s",
                        p->filepath, p->line, p->ifname);
                p->errors++;
                return NULL;
        }
        iface = os_zalloc(sizeof(*iface));
        if (iface == NULL) {
                wpa_printf(MSG_ERROR, "Malloc error!");
                p->errors++;
                return NULL;
        }
        iface->bridge = bridge;
        memcpy(iface->name, p->ifname, sizeof(iface->name));
        if (bridge->ifaces) {
                iface->next = bridge->ifaces;
                iface->prev = iface->next->prev;
                iface->next->prev = iface;
                iface->prev->next = iface;
        } else {
                iface->next = iface->prev = iface;
        }
        bridge->ifaces = iface;    /* point to "current" iface */
        return iface;
}


static void topology_parse_bridge(
        struct topology_parse *p
        )
{
        char *s;
        struct topology_bridge *bridge;
        int error;

        /* Remember the bridge */
        bridge = topology_add_bridge(p);
        if (bridge == NULL) return;

        /* Find leading brace */
        if (topology_find_section(p)) return;

        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "interface")) {
                        s = word_next(s);
                        error = word_get(
                                s, p->ifname, sizeof(p->ifname));
                        if (error) {
                                wpa_printf(MSG_ERROR, 
                                        "File %s line %d Bad interface name",
                                        p->filepath, p->line);
                                p->errors++;
                                strcpy(p->ifname, "?");
                        } else {
                                topology_add_iface(p);
                        }
                        continue;
                }
                /* skip unknown */
        }
        return;
}

static void topology_parse_sta(
        struct topology_parse *p
        )
{
        char *s;
        struct wpa_interface *wiface;
        struct topology_iface *iface;

        iface = topology_find_iface(p, p->ifname);
        if (iface == NULL) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Undeclared iface %s",
                        p->filepath, p->line, p->ifname);
                p->errors++;
                return;
        }
        if (iface->used) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Already used iface %s",
                        p->filepath, p->line, p->ifname);
                p->errors++;
                return;
        }
        iface->used++;

        /* Find leading brace */
        if (topology_find_section(p)) return;

        /* Begin new interface */
        wiface = os_zalloc(sizeof(*wiface));
        if (wiface == NULL) {
                wpa_printf(MSG_ERROR, "Malloc error");
                return;
        }
        wiface->ifname = strdup(iface->name);
        if (strcmp(iface->bridge->name,"none") != 0)
                wiface->bridge_ifname = strdup(iface->bridge->name);
        p->wiface = wiface;

        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == 0) continue;
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "config")) {
                    s = word_next(s);
                    if (word_get( s, p->config_filepath, 
                                    sizeof(p->config_filepath))) {
                            p->errors++;
                            wpa_printf(MSG_ERROR, "File %s line %d"
                                    " config requires path",
                                    p->filepath, p->line);
                    } else {
                            if (wiface->confname) {
                                    p->errors++;
                                    wpa_printf(MSG_ERROR, "File %s line %d"
                                            " duplicate config!",
                                            p->filepath, p->line);
                            } else {
                                    wiface->confname = strdup(p->config_filepath);
                                    printf("Station %s config file %s\n",
                                            p->ifname, wiface->confname);
                            }
                    }
                    continue;
                }
                if (word_eq(s, "driver")) {
                    s = word_next(s);
                    if (word_get( s, p->driver, 
                                    sizeof(p->driver))) {
                            p->errors++;
                            wpa_printf(MSG_ERROR, "File %s line %d"
                                    " driver requires value",
                                    p->filepath, p->line);
                    } else {
                            if (wiface->driver) {
                                    p->errors++;
                                    wpa_printf(MSG_ERROR, "File %s line %d"
                                            " duplicate driver!",
                                            p->filepath, p->line);
                            } else {
                                    wiface->driver = strdup(p->driver);
                            }
                    }
                    continue;
                }
                if (word_eq(s, "ctrl_interface")) {
                    s = word_next(s);
                    if (word_get( s, p->ctrl_interface, 
                                    sizeof(p->ctrl_interface))) {
                            p->errors++;
                            wpa_printf(MSG_ERROR, "File %s line %d"
                                    " ctrl_interface requires value",
                                    p->filepath, p->line);
                    } else {
                            if (wiface->ctrl_interface) {
                                    p->errors++;
                                    wpa_printf(MSG_ERROR, "File %s line %d"
                                            " duplicate ctrl_interface!",
                                            p->filepath, p->line);
                            } else {
                                    wiface->ctrl_interface = strdup(p->ctrl_interface);
                            }
                    }
                    continue;
                }
                /* skip unknown */
        }
        if (wiface->confname == NULL) {
                p->errors++;
                wpa_printf(MSG_ERROR, "File %s line %d No config filepath found",
                        p->filepath, p->line);
        }
        if (wiface->driver == NULL) {
                wiface->driver = strdup("atheros");
        }
        if (p->errors) {
                wpa_printf(MSG_ERROR, "Due to errors, not using sta configuration");
        } else {
                wpa_supplicant_add_iface(p->global, wiface);
        }
        return;
}

static void topology_parse_radio(
        struct topology_parse *p
        )
{
        char *s;
        int error;
        /* Find leading brace */
        if (topology_find_section(p)) return;
        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "sta")) {
                        s = word_next(s);
                        error = word_get(
                                s, p->ifname, sizeof(p->ifname));
                        if (error) {
                                strcpy(p->ifname, "?");
                        } else {
                                topology_parse_sta(p);
                        }
                        continue;
                }
                /* skip unknown */
        }
        return;
}

static void topology_find_radios(
        struct topology_parse *p
        )
{
        char *s;
        int error;
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (word_eq(s, "bridge")) {
                        s = word_next(s);
                        error = word_get(
                                s, p->bridge_name, sizeof(p->bridge_name));
                        if (error) {
                                wpa_printf(MSG_ERROR, 
                                        "File %s line %d Bad bridge name",
                                        p->filepath, p->line);
                                p->errors++;
                                strcpy(p->bridge_name, "?");
                        } else {
                                topology_parse_bridge(p);
                        }
                        continue;
                }
                if (word_eq(s, "radio")) {
                        s = word_next(s);
                        error = word_get(
                                s, p->radio_name, sizeof(p->radio_name));
                        if (error) {
                                strcpy(p->radio_name, "?");
                        } else {
                                topology_parse_radio(p);
                        }
                        continue;
                }
                /* skip unknown */
        }
        return;
}


static void topology_clean(
        struct topology_parse *p
        )
{
        /* Only do normal cleaning (in case of no errors) */
        struct topology_bridge *bridge = p->bridges;
        struct topology_bridge *first_bridge = bridge;
        if (bridge) 
        do {
                struct topology_bridge *next_bridge = bridge->next;
                struct topology_iface *iface = bridge->ifaces;
                struct topology_iface *first_iface = iface;
                if (iface)
                do {
                        struct topology_iface *next_iface = iface->next;
                        free(iface);
                        iface = next_iface;
                } while (iface != first_iface);
                free(bridge);
                bridge = next_bridge;
        } while (bridge != first_bridge);
        free(p);
        return;
}

/* wpa_supplicant_config_read_topology_file reads a topology file,
 * which defines radios and virtual aps, each of which have separate
 * config files.
 * Returns nonzero if error.
 */
int wpa_supplicant_config_read_topology_file(
        struct wpa_global *global,
        char *filepath
        )
{
        struct topology_parse *p;
        int errors;

        printf("Reading topology file %s ...\n", filepath);
        p = malloc(sizeof(*p));
        if (p == NULL) {
                wpa_printf(MSG_ERROR, "Malloc failure.");
                return 1;
        }
        memset(p, 0, sizeof(*p));
        p->global = global;
        p->filepath = filepath;
        p->f = fopen(p->filepath, "r");
        if (p->f == NULL) {
                wpa_printf(MSG_ERROR, "Failed to open topology file: %s",
                        filepath);
                errors = 1;
        } else {
                topology_find_radios(p);
                fclose(p->f);
                errors = p->errors;
        }

        /* Don't bother to clean up if errors... we'll abort anyway */
        if (errors) return errors;
        topology_clean(p);
        return 0;
}


/* wpa_supplicant_config_read_topology_files reads one or more topology files,
 * which define radios and virtual aps, each of which have separate
 * config files.
 * Returns nonzero if error.
 *
 * BUG: sanity checking between topology files is missing.
 */
int wpa_supplicant_config_read_topology_files(
        struct wpa_global *global,
        char **filepaths
        )
{
        int errors = 0;
        char *filepath;
        while ((filepath = *filepaths++) != NULL) {
                errors += wpa_supplicant_config_read_topology_file(
                        global, filepath);
        }
        return errors;
}


