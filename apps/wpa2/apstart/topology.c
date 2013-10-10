/* THIS FILE IS #include'd into apstart.c ! */

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
 *      ap 
 *      {
 *              bss <interface_name>
 *              {
 *              }
 *      }
 *      sta <interface_name>
 *      {
 *      }
 * }
 *
 * radio, ap, sta may be repeated within their section.
 * The special bridge name "none" is allowed (to not use a bridge
 * with the interface(s)).
 */

#include "includes.h"
#include "common.h"
#include <linux/if.h>                   /* for IFNAMSIZ etc. */

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
        int phoney;     /* for bridge "none" etc. */
        char name[IFNAMSIZ+1];
        char ipaddress[16];
        char ipmask[16];
        /* Sibling bridges (linked list) */
        struct topology_bridge *next;
        struct topology_bridge *prev;
        /* child interfaces */
        struct topology_iface *ifaces;
};
struct topology_iface {
        char name[IFNAMSIZ+1];
        char radio_name[IFNAMSIZ+1];
        char type_name[21];
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
        char radio_name[IFNAMSIZ+1];
        char type_name[21];
        char ifname[IFNAMSIZ+1];
        char bridge_name[IFNAMSIZ+1];
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
        if (!strcmp(bridge->name, "none")) {
                bridge->phoney = 1;
        }
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
                if (word_eq(s, "ipaddress")) {
                        s = word_next(s);
                        error = word_get(
                                s, bridge->ipaddress, 
                                sizeof(bridge->ipaddress));
                        if (error) {
                                wpa_printf(MSG_ERROR, 
                                        "File %s line %d Bad bridge ipaddress",
                                        p->filepath, p->line);
                                p->errors++;
                                bridge->ipaddress[0] = 0;
                        }
                        continue;
                }
                if (word_eq(s, "ipmask")) {
                        s = word_next(s);
                        error = word_get(
                                s, bridge->ipmask, 
                                sizeof(bridge->ipmask));
                        if (error) {
                                wpa_printf(MSG_ERROR, 
                                        "File %s line %d Bad bridge ipmask",
                                        p->filepath, p->line);
                                p->errors++;
                                bridge->ipmask[0] = 0;
                        }
                        continue;
                }
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
        memcpy(iface->radio_name, p->radio_name, sizeof(iface->radio_name));
        memcpy(iface->type_name, p->type_name, sizeof(iface->type_name));

        /* Find leading brace */
        if (topology_find_section(p)) return;

        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == 0) continue;
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                /* skip unknown */
        }
        if (p->errors) {
                wpa_printf(MSG_ERROR, "Due to errors, not using sta configuration");
        } else {
        }
        return;
}

static void topology_parse_bss(
        struct topology_parse *p
        )
{
        /* p->ifname contains bss name */
        char *s;
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
        memcpy(iface->radio_name, p->radio_name, sizeof(iface->radio_name));
        memcpy(iface->type_name, p->type_name, sizeof(iface->type_name));

        /* Find leading brace */
        if (topology_find_section(p)) return;
        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                /* skip unknown */
        }
        return;
}



static void topology_parse_ap(
        struct topology_parse *p
        )
{
        char *s;

        /* Find leading brace */
        if (topology_find_section(p)) return;
        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == 0) continue;
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "bss")) {
                        s = word_next(s);
                        if (word_get(s, p->ifname, sizeof(p->ifname))) {
                                p->errors++;
                                wpa_printf(MSG_ERROR, "File %s line %d"
                                        " bss requires interface name",
                                        p->filepath, p->line);
                    }
                    topology_parse_bss(p);
                    continue;
                }
                /* skip unknown */
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
                        strcpy(p->type_name, "sta");
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
                if (word_eq(s, "ap")) {
                    strcpy(p->type_name, "ap");
                    topology_parse_ap(p);
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

/* TODO remove use of "system", use direct subroutine calls.
 */

static int dry_run;

static int my_system(char *cmd, int catcherror)
{
        printf("%s\n", cmd);
        if (!dry_run) {
                if (system(cmd)) {
                        if (catcherror) {
                                printf("ERROR: Command failed: %s\n", cmd);
                                return 1;
                        }
                }
        }
        return 0;
}

static int iface_down(char *name)
{
        char buf[200];
        sprintf(buf, "ifconfig %s down", name);
        return my_system(buf, 0);
}

static int iface_addr_set(char *name, char *addr)
{
        char buf[200];
        sprintf(buf, "ifconfig %s %s", name, addr);
        return my_system(buf, 1);
}

static int iface_addr_clear(char *name)
{
        char buf[200];
        sprintf(buf, "ifconfig %s 0.0.0.0", name);
        return my_system(buf, 0);
}

static int iface_mask_set(char *name, char *mask)
{
        char buf[200];
        sprintf(buf, "ifconfig %s netmask %s", name, mask);
        return my_system(buf, 1);
}

static int iface_up(char *name)
{
        char buf[200];
        sprintf(buf, "ifconfig %s up", name);
        return my_system(buf, 1);
}

static int bridge_delete(char *name)
{
        char buf[200];
        sprintf(buf, "brctl delbr %s", name);
        return my_system(buf, 0);
}

static int bridge_create(char *name)
{
        char buf[200];
        sprintf(buf, "brctl addbr %s", name);
        return my_system(buf, 1);
}

static int bridge_addif(char *name, char *ifname)
{
        char buf[200];
        sprintf(buf, "brctl addif %s %s", name, ifname);
        return my_system(buf, 1);
}

static int iface_destroy(char *name)
{
        char buf[200];
        sprintf(buf, "wlanconfig %s destroy", name);
        return my_system(buf, 0);
}

static int iface_create(
                char *name,
                char *type,             /* "sta", "ap" etc. */
                char *wifi_name         /* e.g. wifi0 */
                )
{
        char buf[200];
        sprintf(buf, "wlanconfig %s create nounit wlandev %s wlanmode %s %s",
            name, wifi_name, type,
            (!strcmp(type,"sta") ? "nosbeacon" : ""));
        return my_system(buf, 1);
}


static int bridge_setfd(char *name)    /* set forward delay */
{
        char buf[200];
        sprintf(buf, "brctl setfd %s 1", name);
        return my_system(buf, 1);
}


static void topology_setup(
        struct topology_parse *p,
        int bringup
        )
{
        struct topology_bridge *bridge;
        struct topology_bridge *first_bridge;
        int iath;

        /* Take down all of the interfaces and delete bridges */
        bridge = p->bridges;
        first_bridge = bridge;
        if (bridge != NULL)
        do {
                struct topology_iface *iface = bridge->ifaces;
                struct topology_iface *first_iface = iface;
                if (iface != NULL) 
                do {
                        p->errors += iface_down(iface->name);
                        iface = iface->next;
                } while (iface != first_iface);
                if (! bridge->phoney) {
                        (void) iface_down(bridge->name);
                        (void) bridge_delete(bridge->name);
                }
                bridge = bridge->next;
        } while (bridge != first_bridge);

        /* Delete ath devices */
        bridge = p->bridges;
        first_bridge = bridge;
        if (bridge != NULL)
        do {
                struct topology_iface *iface = bridge->ifaces;
                struct topology_iface *first_iface = iface;
                if (iface != NULL) 
                do {
                        if (iface->name[0] == 'a' &&
                            iface->name[1] == 't' &&
                            iface->name[2] == 'h' &&
                            isdigit(iface->name[3])) {
                                (void) iface_destroy(iface->name);
                        }
                        iface = iface->next;
                } while (iface != first_iface);
                bridge = bridge->next;
        } while (bridge != first_bridge);

        if (!bringup) return;

        /* Recreate ath devices.
         * Due to challenged drivers, this needs to be done in order
         * (ath0, then ath1 etc.)
         */
        for (iath = 0; iath < 8; iath++) {
                bridge = p->bridges;
                first_bridge = bridge;
                if (bridge != NULL)
                do {
                        struct topology_iface *iface = bridge->ifaces;
                        struct topology_iface *first_iface = iface;
                        if (iface != NULL) 
                        do {
                                if (iface->used &&
                                    iface->name[0] == 'a' &&
                                    iface->name[1] == 't' &&
                                    iface->name[2] == 'h' &&
                                    iface->name[3] == ('0'+iath)) {
                                        p->errors += iface_create(iface->name, 
                                                iface->type_name, 
                                                iface->radio_name);
                                }
                                iface = iface->next;
                        } while (iface != first_iface);
                        bridge = bridge->next;
                } while (bridge != first_bridge);
        }

        /* Bring up all of the interfaces and create bridges */
        bridge = p->bridges;
        first_bridge = bridge;
        if (bridge != NULL)
        do {
                struct topology_iface *iface = bridge->ifaces;
                struct topology_iface *first_iface = iface;
                if (!bridge->phoney) {
                        p->errors += bridge_create(bridge->name);
                }
                if (iface != NULL) 
                do {
                        /* Clear IP address so routing tables get fixed */
                        p->errors += iface_addr_clear(iface->name);
                        p->errors += iface_up(iface->name);
                        p->errors += bridge_addif(bridge->name, iface->name);
                        iface = iface->next;
                } while (iface != first_iface);
                if (!bridge->phoney) {
                        if (bridge->ipaddress[0]) {
                                p->errors += iface_addr_set(
                                        bridge->name, bridge->ipaddress);
                        }
                        if (bridge->ipmask[0]) {
                                p->errors += iface_mask_set(
                                        bridge->name, bridge->ipmask);
                        }
                        p->errors += iface_up(bridge->name);
                        p->errors += bridge_setfd(bridge->name);
                }
                bridge = bridge->next;
        } while (bridge != first_bridge);

        return;
}

/* apstart_read_topology_file reads a topology file,
 * which defines radios and virtual aps, each of which have separate
 * config files.
 * Returns nonzero if error.
 */
int apstart_read_topology_file(
        char *filepath,
        int bringup,    /* 1 to bring up, 0 to take down */
        int dry_run_flag     /* 1 to not really do anything */
        )
{
        struct topology_parse *p;
        int errors;

        dry_run = dry_run_flag;

        printf("Reading topology file %s ...\n", filepath);
        p = malloc(sizeof(*p));
        if (p == NULL) {
                wpa_printf(MSG_ERROR, "Malloc failure.");
                return 1;
        }
        memset(p, 0, sizeof(*p));
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

        /* Now set up (or take down) interfaces as required */
        topology_setup(p, bringup);

        topology_clean(p);
        return errors;
}


