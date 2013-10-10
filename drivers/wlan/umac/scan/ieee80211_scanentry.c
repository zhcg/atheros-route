/*
 *  Copyright (c) 2008 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ieee80211_scan_priv.h>

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#if UMAC_SUPPORT_SCAN

/*
 * Scan cache entry format used when exporting data from a policy
 * module; this data may be represented some other way internally.
 */
struct ieee80211_scan_entry {
    TAILQ_ENTRY(ieee80211_scan_entry) se_list;
    LIST_ENTRY(ieee80211_scan_entry)  se_hash;

    spinlock_t                        se_lock;            /* synchronization object */
#if IEEE80211_DEBUG_REFCNT_SE
#define NUM_TRACE_BUF_SE           (1 << 5)    /*32 recent adds and removes */
        const char  *t_func[NUM_TRACE_BUF_SE];   /*who call s it */
        const char  *t_self[NUM_TRACE_BUF_SE];   /*add or remove */
        int         t_line[NUM_TRACE_BUF_SE];    /*line where it's called */
        int         t_refcnt[NUM_TRACE_BUF_SE];  /*current ref count */
        int         t_nth[NUM_TRACE_BUF_SE];     /*which number called */
        atomic_t    t_calls;                     /*total number called */
        atomic_t    t_index;                     /*array index of this trace */
#endif
    atomic_t                          se_refcount;        /* reference count */

    u_int32_t                         se_phy_mode;        /* PHY mode */
    int8_t                            se_rssi;            /* absolute rssi */
    u_int32_t                         se_avgrssi;         /* avg'd recv ssi */

    u_int8_t                          se_macaddr[IEEE80211_ADDR_LEN];
    u_int8_t                          se_bssid[IEEE80211_ADDR_LEN];
    u_int8_t                          se_ssid[2+IEEE80211_NWID_LEN];
    u_int32_t                         se_timestamp;       /* beacon/probe response timestamp */
    u_int32_t                         se_rssi_timestamp;  /* rssi timestamp */
    struct ieee80211_channel          *se_chan;           /* channel where sta found */
    union {
        u_int8_t     data[8];
        u_int64_t    tsf;
    } se_tsf;                                             /* from last rcv'd beacon */
    u_int16_t                         se_intval;          /* beacon interval (host byte order) */
    u_int16_t                         se_capinfo;         /* capabilities (host byte order) */
    u_int16_t                         se_timoff;          /* byte offset to TIM ie */
    u_int16_t                         se_fhdwell;         /* FH only (host byte order) */
    u_int8_t                          se_fhindex;         /* FH only */
    u_int8_t                          se_erp;             /* ERP from beacon/probe resp */
    u_int8_t                          se_dtimperiod;      /* DTIM period */

    /* Information Elements and copy of beacon frame */
    u_int8_t                          *se_beacon_data;    /* complete beacon frame  */
    u_int16_t                         se_beacon_len;      /* length of beacon frame */
    u_int16_t                         se_beacon_alloc;    /* allocated size for beacon frame */

    u_int8_t                          *se_ie_data;        /* all the tagged ie's  */
    u_int16_t                         se_ie_len;          /* length of all tagged ies */

    struct ieee80211_ie_list          se_ie_list;

    /* 
     * Additional information to keep track of WCN IE. 
     * According to Microsoft, the WCN IE's from beacon and probe response frames are different.
     */
    int                               se_subtype;         /* frame subtype of current copy of IE data */
    u_int8_t                          *se_alt_wcn_ie;     /* WCN IE copy of the other subtype frame */
    int                               se_alt_wcn_ie_len;  /* len of alternate WCN IE (include IE Header) */

    /* MLME-related information  */
    u_int8_t                          se_fails;           /* failure to associate count */
    systime_t                         se_lastfail;        /* time of last failure  in msec*/
    systime_t                         se_lastassoc;       /* time of last association in msec */

    /* Information used to calculate scan entry rank */
    systime_t                         se_bad_ap_time;     /* time when AP was marked 'bad' */
    u_int32_t                         se_assoc_cost;      /* association cost */ 
    u_int8_t                          se_demerit_utility; /* added rank to preferred bss's */
    u_int32_t                         se_pref_bss_rank;   /* added rank to preferred bss's */
    u_int32_t                         se_status;          /* opaque status word */ 
    u_int32_t                         se_rank;            /* rank */
    u_int32_t                         se_utility;         /* utility */
    u_int32_t                         se_assoc_state;     /* opaque association state word */ 
    u_int32_t                         se_chanload;        /* channel load */
    systime_t                         se_radar_detected_timestamp;  /* time when AP was marked 'radar' */
    u_int32_t                         se_csa_delay;       /* AP was marked 'radar' for at least this period */
    systime_t                         se_lastdeauth;      /* last deauth/disassoc time */
};

struct ieee80211_scan_table {
    struct ieee80211com                     *st_ic;
    osdev_t                                 st_osdev;
    spinlock_t                              st_lock;     /* on scan table */
    TAILQ_HEAD(, ieee80211_scan_entry)      st_entry;    /* all entries */
    ATH_LIST_HEAD(,ieee80211_scan_entry)    st_hash[STA_HASHSIZE];
};

#define IEEE80211_SCANENTRY_PRINTF(_ic, _cat, _fmt, ...)    \
    if ((_ic)->ic_debug & (_cat)) {                         \
        ieee80211com_note((_ic), _fmt, __VA_ARGS__);        \
    }

#if IEEE80211_DEBUG_REFCNT_SE
#define TRACE_ENTRY(_entry, _self, _func, _line) do {                    \
    atomic_t index = atomic_read(&(_entry)->t_index) & (NUM_TRACE_BUF_SE - 1);\
    atomic_inc(&(_entry)->t_index);                                       \
    (_entry)->t_func[index] = _func;                                      \
    (_entry)->t_self[index] = _self;                                      \
    (_entry)->t_line[index] = _line;                                      \
    (_entry)->t_refcnt[index] = atomic_read(&(_entry)->se_refcount);      \
    (_entry)->t_nth[index] = atomic_read(&(_entry)->t_calls);             \
    atomic_inc(&(_entry)->t_calls);                                      \
} while (0)

#define IEEE80211_TRACE_HISTORYSE(_entry, self, func, line)   \
    TRACE_ENTRY(_entry, self, func, line)
    
#define IEEE80211_TRACE_CREATESE(_entry, func, line) \
    TRACE_ENTRY(_entry, func, func, line)

#define IEEE80211_DUMP_TRACESE(_entry) do {          \
    int i = 0;                                            \
    for (; i < NUM_TRACE_BUF_SE && (_entry)->t_func[i]; i++) {     \
        printk("***** %s(%d,%d)=> called by %s:%d, SSID=%.*s,refcnt:%d,ptr:0x%p\n", \
           (_entry)->t_self[i], i,                      \
           (_entry)->t_nth[i], (_entry)->t_func[i],               \
           (_entry)->t_line[i], (_entry)->se_ssid[1],       \
           &(_entry)->se_ssid[2], (_entry)->t_refcnt[i],      \
           (_entry));                                      \
    }                                                       \
} while (0)

#define IEEE80211_TRACE_ADDSE(_entry, self, func, line) do { \
    printk("***** %s(called by %s:%d): (MAC:%02X:%02X:%02X:%02X:%02X:%02X) SSID=%.*s, refcnt from %d to %d\n", \
        self, func, line,                               \
        (_entry)->se_macaddr[0],(_entry)->se_macaddr[1],     \
        (_entry)->se_macaddr[2], (_entry)->se_macaddr[3],    \
        (_entry)->se_macaddr[4], (_entry)->se_macaddr[5],    \
        (_entry)->se_ssid[1], &(_entry)->se_ssid[2],          \
        atomic_read(&(_entry)->se_refcount),                  \
        atomic_read(&(_entry)->se_refcount) + 1);            \
} while (0)

#define IEEE80211_TRACE_REMSE(_entry, self, func, line) do { \
     printk("***** %s(called by %s:%d): (MAC:%02X:%02X:%02X:%02X:%02X:%02X) SSID=%.*s, refcnt from %d to %d\n",  \
        self, func, line,                                 \
        (_entry)->se_macaddr[0],(_entry)->se_macaddr[1],     \
        (_entry)->se_macaddr[2], (_entry)->se_macaddr[3],     \
        (_entry)->se_macaddr[4],(_entry)->se_macaddr[5],      \
        (_entry)->se_ssid[1], &(_entry)->se_ssid[2],        \
        atomic_read(&(_entry)->se_refcount),                \
        atomic_read(&(_entry)->se_refcount) - 1);            \
} while (0)


#else
#define IEEE80211_TRACE_CREATESE(_entry, func, line)
#define IEEE80211_DUMP_TRACESE(_entry)
#define IEEE80211_TRACE_REMSE(_entry, self, func, line)
#define IEEE80211_TRACE_ADDSE(_entry, self, func, line)
#define IEEE80211_TRACE_HISTORYSE(_entry, self, func, line)

#endif

/*
 * functions to fetch data from scan entry object 
 */

u_int8_t ieee80211_scan_entry_reference_count(ieee80211_scan_entry_t scan_entry)
{   
    return atomic_read(&scan_entry->se_refcount);
}

#define REFERENCE_COUNT_THRESHOLD    255

u_int8_t ieee80211_scan_entry_add_reference_dbg(
    ieee80211_scan_entry_t scan_entry, const char *func, int line)
{

#if !IEEE80211_DEBUG_REFCNT_SE
    UNREFERENCED_PARAMETER(func);
    UNREFERENCED_PARAMETER(line);
#endif


    if (atomic_read(&scan_entry->se_refcount) >= REFERENCE_COUNT_THRESHOLD) {
        /* Something went wrong */
        IEEE80211_DUMP_TRACESE(scan_entry);
        ASSERT(0);
        return atomic_read(&scan_entry->se_refcount);
    }

    IEEE80211_TRACE_ADDSE(scan_entry, __func__, func, line);
    /* return updated count */
    atomic_inc(&scan_entry->se_refcount);/* don't return it directly */
    IEEE80211_TRACE_HISTORYSE(scan_entry, __func__, func, line);
        
    return atomic_read(&scan_entry->se_refcount);
}

u_int8_t ieee80211_scan_entry_remove_reference_dbg(
    ieee80211_scan_entry_t scan_entry, const char *func, int line)
{

#if !IEEE80211_DEBUG_REFCNT_SE
    UNREFERENCED_PARAMETER(func);
    UNREFERENCED_PARAMETER(line);
#endif

    if (atomic_read(&scan_entry->se_refcount) == 0) {
        /* Something went wrong */
        IEEE80211_DUMP_TRACESE(scan_entry);
        ASSERT(0);
        return atomic_read(&scan_entry->se_refcount);
    }
 
    IEEE80211_TRACE_REMSE(scan_entry, __func__, func, line);
    /* return updated count */
    atomic_dec(&scan_entry->se_refcount); /* don't return it directly */
    IEEE80211_TRACE_HISTORYSE(scan_entry, __func__, func, line);
   
    return atomic_read(&scan_entry->se_refcount);

}

void ieee80211_scan_entry_lock(ieee80211_scan_entry_t scan_entry)
{
    spin_lock(&(scan_entry->se_lock));
}

void ieee80211_scan_entry_unlock(ieee80211_scan_entry_t scan_entry)
{
    spin_unlock(&(scan_entry->se_lock));
}

u_int8_t * ieee80211_scan_entry_macaddr(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_macaddr;
}

u_int8_t * ieee80211_scan_entry_bssid(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_bssid;
}

u_int32_t ieee80211_scan_entry_timestamp(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_timestamp;
}

void ieee80211_scan_entry_reset_timestamp(ieee80211_scan_entry_t scan_entry)
{
    scan_entry->se_timestamp = 0;
}

u_int8_t *ieee80211_scan_entry_tsf(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_tsf.data;
}

u_int16_t ieee80211_scan_entry_capinfo(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_capinfo;
}

u_int16_t ieee80211_scan_entry_beacon_interval(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_intval;
}

u_int8_t ieee80211_scan_entry_rssi(ieee80211_scan_entry_t scan_entry)
{
    u_int32_t    rssi = ATH_RSSI_OUT(scan_entry->se_avgrssi);
    
    /*
     * An entry is in the BSS list means we've received at least one beacon
     * from the corresponding AP, so the rssi must be initialized.
     *
     * If the RSSI is not initialized, return 0 (i.e. RSSI == Noise Floor).
     * Once se_avgrssi field has been initialized, ATH_RSSI_OUT always returns
     * values that fit in an 8-bit variable (RSSI values are typically 0-90.
     */
    return (rssi == ATH_RSSI_DUMMY_MARKER) ? 0 : (u_int8_t) rssi;
}

u_int8_t *ieee80211_scan_entry_ssid(ieee80211_scan_entry_t scan_entry, u_int8_t *len)
{
    if (scan_entry->se_ssid[1] == 0) {
        *len = 0;
        return NULL;
    }

    *len =  scan_entry->se_ssid[1]; 
    return (((*len) > 0) ? &(scan_entry->se_ssid[2]) : NULL);
}

u_int8_t ieee80211_scan_entry_dtimperiod(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_dtimperiod;
}

u_int8_t *ieee80211_scan_entry_tim(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.tim;
}

u_int8_t *ieee80211_scan_entry_beacon_data(ieee80211_scan_entry_t scan_entry, u_int16_t *beacon_len)
{
    *beacon_len = scan_entry->se_beacon_len;

    return scan_entry->se_beacon_data;
}

u_int16_t ieee80211_scan_entry_beacon_len(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_beacon_len;
}

u_int8_t *ieee80211_scan_entry_ie_data(ieee80211_scan_entry_t scan_entry, u_int16_t *ie_len)
{
    *ie_len = scan_entry->se_ie_len;

    return scan_entry->se_ie_data;
}

u_int16_t ieee80211_scan_entry_ie_len(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_len;
}

struct ieee80211_channel *ieee80211_scan_entry_channel( ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_chan;
}

u_int8_t ieee80211_scan_entry_erpinfo(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_erp;
}

u_int8_t *ieee80211_scan_entry_rates(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.rates;
}

u_int8_t *ieee80211_scan_entry_xrates(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.xrates;
}

u_int8_t *ieee80211_scan_entry_rsn(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.rsn;
}

u_int8_t *ieee80211_scan_entry_wpa(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.wpa;
}

u_int8_t *ieee80211_scan_entry_wps(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.wps;
}

#if ATH_SUPPORT_WAPI
u_int8_t *ieee80211_scan_entry_wapi(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.wapi;
}
#endif

u_int8_t *ieee80211_scan_entry_sfa(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.sfa;
}

u_int8_t *ieee80211_scan_entry_csa(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.csa;
}

u_int8_t *ieee80211_scan_entry_xcsa(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.xcsa;
}

u_int8_t *ieee80211_scan_entry_htinfo(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.htinfo;
}

u_int8_t *ieee80211_scan_entry_htcap(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.htcap;
}

u_int8_t *ieee80211_scan_entry_quiet(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.quiet;
}

u_int8_t *ieee80211_scan_entry_qbssload(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.qbssload;
}

u_int8_t *ieee80211_scan_entry_vendor(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.vendor;
}

u_int8_t *ieee80211_scan_entry_country(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.country;
}

u_int8_t *ieee80211_scan_entry_wmeinfo_ie(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.wmeinfo;
}

u_int8_t *ieee80211_scan_entry_wmeparam_ie(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.wmeparam;
}

#if ATH_SUPPORT_IBSS_DFS
u_int8_t *ieee80211_scan_entry_ibssdfs_ie(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_ie_list.ibssdfs;
}
#endif /* ATH_SUPPORT_IBSS_DFS */

/*
 * age of the scan entry in msec.
 */
u_int32_t ieee80211_scan_entry_age(ieee80211_scan_entry_t scan_entry)
{
    systime_t    time_stamp = scan_entry->se_timestamp;

    /* 
     * Save timestamp before querying current time to avoid negative values
     * caused by preemption.
     */
    return ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - time_stamp));
}

u_int32_t ieee80211_scan_entry_status(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_status;
}

void ieee80211_scan_entry_set_status(ieee80211_scan_entry_t scan_entry, u_int32_t status)
{
    scan_entry->se_status = status;
}

u_int32_t ieee80211_scan_entry_assoc_state(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_assoc_state;
}

void ieee80211_scan_entry_set_assoc_state(ieee80211_scan_entry_t scan_entry, u_int32_t state)
{
    scan_entry->se_assoc_state = state;
}

systime_t ieee80211_scan_entry_bad_ap_time(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_bad_ap_time;
}

void ieee80211_scan_entry_set_bad_ap_time(ieee80211_scan_entry_t scan_entry, systime_t timestamp)
{
    scan_entry->se_bad_ap_time = timestamp;
}

bool ieee80211_scan_entry_is_radar_detected_period(ieee80211_scan_entry_t scan_entry)
{
    return (scan_entry->se_radar_detected_timestamp != 0);
}

void ieee80211_scan_entry_set_radar_detected_timestamp(ieee80211_scan_entry_t scan_entry, systime_t timestamp, u_int32_t csa_delay)
{
    scan_entry->se_radar_detected_timestamp = timestamp;
    scan_entry->se_csa_delay = csa_delay;
}

systime_t ieee80211_scan_entry_lastassoc(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_lastassoc;
}

void ieee80211_scan_entry_set_lastassoc(ieee80211_scan_entry_t scan_entry, systime_t timestamp)
{
    scan_entry->se_lastassoc = timestamp;
}

systime_t ieee80211_scan_entry_lastdeauth(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_lastdeauth;
}

void ieee80211_scan_entry_set_lastdeauth(ieee80211_scan_entry_t scan_entry, systime_t timestamp)
{
    scan_entry->se_lastdeauth= timestamp;
}

u_int32_t ieee80211_scan_entry_assoc_cost(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_assoc_cost;
}

void ieee80211_scan_entry_set_assoc_cost(ieee80211_scan_entry_t scan_entry, u_int32_t cost)
{
    scan_entry->se_assoc_cost = cost;
}

u_int32_t ieee80211_scan_entry_rank(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_rank;
}

void ieee80211_scan_entry_set_rank(ieee80211_scan_entry_t scan_entry, u_int32_t rank)
{
    scan_entry->se_rank = rank;
}

u_int32_t ieee80211_scan_entry_pref_bss_rank(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_pref_bss_rank;
}

void ieee80211_scan_entry_set_pref_bss_rank(ieee80211_scan_entry_t scan_entry, u_int32_t rank)
{
    scan_entry->se_pref_bss_rank = rank;
}

u_int8_t ieee80211_scan_entry_demerit_utility(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_demerit_utility;
}

void ieee80211_scan_entry_set_demerit_utility(ieee80211_scan_entry_t scan_entry, u_int8_t enable)
{
    scan_entry->se_demerit_utility = enable;
}

u_int32_t ieee80211_scan_entry_utility(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_utility;
}

void ieee80211_scan_entry_set_utility(ieee80211_scan_entry_t scan_entry, u_int32_t utility)
{
    scan_entry->se_utility = utility;
}

u_int32_t ieee80211_scan_entry_chanload(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_chanload;
}

void ieee80211_scan_entry_set_chanload(ieee80211_scan_entry_t scan_entry, u_int32_t load)
{
    scan_entry->se_chanload = load;
}

u_int32_t ieee80211_scan_entry_phymode(ieee80211_scan_entry_t scan_entry)
{
    return scan_entry->se_phy_mode;
}

enum ieee80211_opmode ieee80211_scan_entry_bss_type(ieee80211_scan_entry_t scan_entry)
{
    return (scan_entry->se_capinfo & IEEE80211_CAPINFO_ESS) ? IEEE80211_M_STA : IEEE80211_M_IBSS;
}

u_int8_t ieee80211_scan_entry_privacy(ieee80211_scan_entry_t scan_entry)
{
    return ((scan_entry->se_capinfo & IEEE80211_CAPINFO_PRIVACY) != 0);
}

struct ieee80211_ie_athAdvCap *
ieee80211_scan_entry_athcaps(ieee80211_scan_entry_t scan_entry)
{
    return (struct ieee80211_ie_athAdvCap *) scan_entry->se_ie_list.athcaps;
}

struct ieee80211_ie_ath_extcap *
ieee80211_scan_entry_athextcaps(ieee80211_scan_entry_t scan_entry)
{
    return (struct ieee80211_ie_ath_extcap *) scan_entry->se_ie_list.athextcaps;
}

u_int32_t ieee80211_channel_frequency(struct ieee80211_channel *chan)
{
    return chan->ic_freq;    /* frequency in Mhz */
}

u_int32_t ieee80211_channel_ieee(struct ieee80211_channel *chan)
{
    return chan->ic_ieee;    /* channel number */
}

static struct ieee80211_scan_entry *
ieee80211_create_new_scan_entry(
    struct ieee80211vap         *vaphandle,
    const u_int8_t              *macaddr, 
    const u_int8_t              *ssid_ie,
    struct ieee80211_channel    *chan,
    int                         subtype,
    osdev_t                     st_osdev)
{
    struct ieee80211_scan_entry    *scan_entry;

    scan_entry = (struct ieee80211_scan_entry *) 
        OS_MALLOC(st_osdev, sizeof(struct ieee80211_scan_entry), 0);

    if (scan_entry == NULL) {
        IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
            "%s: Failed to create scan entry: %02X:%02X:%02X:%02X:%02X:%02X\n",
            __func__, 
            macaddr[0], macaddr[1], macaddr[2],
            macaddr[3], macaddr[4], macaddr[5]);

        return NULL;
    }

    /* Clear scan entry memory */
    OS_MEMZERO(scan_entry, sizeof(struct ieee80211_scan_entry));

    /* 
     * Initialize basic scan entry information
     */
    IEEE80211_ADDR_COPY(scan_entry->se_macaddr, macaddr);

    /* Validate SSID length before copying. */
    if (ssid_ie != NULL) {
        if (2 + ssid_ie[1] <= sizeof(scan_entry->se_ssid)) {
            OS_MEMCPY(scan_entry->se_ssid, ssid_ie, 2 + ssid_ie[1]);
        }
    }

    scan_entry->se_chan     = chan;
    atomic_set(&scan_entry->se_refcount, 1);
    scan_entry->se_subtype  = subtype;   /* Initial subtype */
    scan_entry->se_avgrssi  = ATH_RSSI_DUMMY_MARKER;
    scan_entry->se_rssi     = 0;

    IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
        "%s: %02X:%02X:%02X:%02X:%02X:%02X SSID=%.*s chan=%3d p=%08p\n",
        __func__, 
        macaddr[0], macaddr[1], macaddr[2],
        macaddr[3], macaddr[4], macaddr[5],
        scan_entry->se_ssid[1],
        &(scan_entry->se_ssid[2]),
        ((scan_entry->se_chan != NULL) ? wlan_channel_ieee(scan_entry->se_chan) : 0),
        scan_entry);

    IEEE80211_TRACE_CREATESE(scan_entry, __func__, __LINE__);
    
    return scan_entry;
}

static u_int8_t
ieee80211_is_hidden_ssid(const struct ieee80211_ie_ssid *ssid_ie)
{
    int    i;

    /*
     * We flag this as Hidden SSID if the Length is 0
     * of the SSID only contains 0's
     */
    if ((ssid_ie == NULL) || (ssid_ie->ssid_len == 0))
    {
        /* Zero length is hidden SSID */
        return true;
    }

    for (i = 0; i < ssid_ie->ssid_len; i++)
    {
        if (ssid_ie->ssid[i] != 0)
        {
            /* Non zero SSID value */
            return false;
        }
    }

    /* All 0's */
    return true;
}

/*
 * Match a scan entry's characteristics against those of a beacon/probe response.
 * Criteria for a scan entry match:
 *     -same MAC address;
 *     -same frequency band (2GHz or 5GHz)
 *     -SSID match (see below)
 *
 * Criteria for SSID match:
 *     -If beacon/probe response has an SSID, report a match only if scan entry
 *      has exactly the same SSID.
 *     -If beacon/probe response has a "hidden" SSID then we report a match  
 *      between the beacon/probe response and the scan entry, and return an
 *      additional flag indicating whether the SSID was matched.
 */
static bool
ieee80211_is_scan_entry_match(const struct ieee80211_scan_entry *scan_entry, 
                              const u_int8_t                    *beacon_macaddr, 
                              const struct ieee80211_ie_ssid    *beacon_ssid_ie, 
                              const struct ieee80211_channel    *beacon_chan,
                              bool                              *pssid_match)
{
    bool    b_found = false;

    if (pssid_match != NULL) {
        *pssid_match = false;
    }

    /*
     * Match MAC address and frequency band
     */
    if (IEEE80211_ADDR_EQ(scan_entry->se_macaddr, beacon_macaddr) &&
        ieee80211_is_same_frequency_band(scan_entry->se_chan, beacon_chan)) {
        /*
         * Match SSID.
         */
        if (ieee80211_is_hidden_ssid(beacon_ssid_ie)) { 
            /*
             * Beacon with hidden SSID matches any scan entry's SSID
             */
            b_found = true;

            /*
             * Check whether beacon's SSID matches scan entry's SSID.
             */
            if (pssid_match != NULL) {
                *pssid_match = ieee80211_is_hidden_ssid((struct ieee80211_ie_ssid *) scan_entry->se_ssid);
            }
        }
        else {
            /*
             * Beacon/Probe Response contains explicit SSID.
             * Scan entry must contain the same exact SSID for a match.
             */
            b_found = ((scan_entry->se_ssid[1] == beacon_ssid_ie->ssid_len) && 
                (! OS_MEMCMP(scan_entry->se_ssid + 2, beacon_ssid_ie->ssid, beacon_ssid_ie->ssid_len)));

            /*
             * Indicate whether SSID matched.
             */
            if (pssid_match != NULL) {
                *pssid_match = b_found;
            }
        }
    }

    return b_found;
}

/*
 * Update a scan entry based on the last received beacon or probe response.
 */
static struct ieee80211_scan_entry *
ieee80211_scan_entry_update(struct ieee80211vap                  *vaphandle,
                            struct ieee80211_scan_entry          *scan_entry,
                            const u_int8_t                       *bssid,
                            bool                                 ssid_match,
                            struct ieee80211_scanentry_params    *scan_entry_parameters,
                            struct ieee80211_beacon_frame        *beacon_frame,
                            int                                  beacon_length,
                            int                                  subtype, 
                            int                                  rssi, 
                            systime_t                            current_time,
                            osdev_t                              st_osdev)
{
    int     beacon_length_alt;
    bool    channel_change = false;

#define AVERAGE_RSSI_TIME_LIMIT    5000    /* 5 seconds */

    /*
     * If the AP has been marked as radar detected and a delay has elapsed,
     * clear its timeout as new beacon received.
     */
    if (scan_entry->se_radar_detected_timestamp &&
		(CONVERT_SYSTEM_TIME_TO_MS(current_time - scan_entry->se_radar_detected_timestamp)
		>= scan_entry->se_csa_delay )) {
        scan_entry->se_radar_detected_timestamp = 0;
    }

    /*
     * Check whether AP has changed channels (BSSID will be the same; SSID
     * may or may not have changed. 
     * We do not want 2 entries that are identical except for the channel 
     * number.
     */
    if (wlan_channel_ieee(scan_entry->se_chan) != wlan_channel_ieee(scan_entry_parameters->chan)) {
        channel_change = true;

        if (! ssid_match)
        {
            IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
                "%s: SSID Mismatch: SSID: Entry=%32.*s Beacon=%32.*s (l=%02d)\n"
                "    MAC=%02X:%02X:%02X:%02X:%02X:%02X BSSID=%02X:%02X:%02X:%02X:%02X:%02X RSSI=%2d avg=%2d Phy=%08X Ch=%3d Mismatch=%d Ref=%2d p=%8p\n",
                __func__,
                min(scan_entry->se_ssid[1], (u_int8_t)32),
                &(scan_entry->se_ssid[2]),
                ((scan_entry_parameters->ie_list.ssid != NULL) ? scan_entry_parameters->ie_list.ssid[1] : 0),
                ((scan_entry_parameters->ie_list.ssid != NULL) ? &(scan_entry_parameters->ie_list.ssid[2]) : NULL),
                ((scan_entry_parameters->ie_list.ssid != NULL) ? scan_entry_parameters->ie_list.ssid[1] : 0),
                scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
                scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
                scan_entry->se_bssid[0], scan_entry->se_bssid[1], scan_entry->se_bssid[2],
                scan_entry->se_bssid[3], scan_entry->se_bssid[4], scan_entry->se_bssid[5],
                rssi, ATH_RSSI_OUT(scan_entry->se_avgrssi), 
                scan_entry->se_phy_mode, wlan_channel_ieee(scan_entry->se_chan), scan_entry_parameters->channel_mismatch,
                atomic_read(&scan_entry->se_refcount),
                scan_entry);

            /*
             * Age out entry if an AP with same BSSID is found on a different
             * channel.
             *
             * *** Do not try to update the SSID, or we will end up with 
             * duplicated entries.
             */
            if (IEEE80211_ADDR_EQ(scan_entry->se_bssid, bssid)) {
                /* **Do not age out entries that are in use, including:
                 *    -Active AP (Home AP)
                 *    -Candidate AP List
                 *    -AP we're trying to connect to
                 *    -etc.
                 * Reference count must be at most 1:
                 *     - one reference from the BSS list
                 */
                if (! ieee80211_mlme_is_connected(vaphandle->iv_ic, scan_entry)) {
                    if (scan_entry->se_timestamp != 0)
                    {
                        IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, "%s", "    **Aged Out**\n");
                    }

                    ieee80211_scan_entry_reset_timestamp(scan_entry);
                }
                else 
                {
                    IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, "%s", "    **Home AP - Do Not Age Out**\n");
                }
            }

            /* SSIDs do not match. */
            return scan_entry;
        }
    }

    if ((beacon_frame->capability & IEEE80211_CAPINFO_IBSS) && (! ssid_match)) {
        /*
         * IBSS restarted with different SSID. Remove old entry.
         * Reference count must be at most 2:
         *     - one reference from the BSS list
         *     - one reference from the routine updating the entry
         */
        if (! ieee80211_mlme_is_connected(vaphandle->iv_ic, scan_entry)) {
            if (scan_entry->se_timestamp > 0) {
                IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
                    "%s: SSID Mismatch **AGE OUT**: SSID: Entry=%32.*s Beacon=%32.*s (l=%02d)\n"
                    "    MAC=%02X:%02X:%02X:%02X:%02X:%02X BSSID=%02X:%02X:%02X:%02X:%02X:%02X RSSI=%2d avg=%2d Phy=%08X Ch=%3d Mismatch=%d Ref=%2d p=%8p\n",
                    __func__,
                    min(scan_entry->se_ssid[1], (u_int8_t)32),
                    &(scan_entry->se_ssid[2]),
                    ((scan_entry_parameters->ie_list.ssid != NULL) ? scan_entry_parameters->ie_list.ssid[1] : 0),
                    ((scan_entry_parameters->ie_list.ssid != NULL) ? &(scan_entry_parameters->ie_list.ssid[2]) : NULL),
                    ((scan_entry_parameters->ie_list.ssid != NULL) ? scan_entry_parameters->ie_list.ssid[1] : 0),
                    scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
                    scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
                    scan_entry->se_bssid[0], scan_entry->se_bssid[1], scan_entry->se_bssid[2],
                    scan_entry->se_bssid[3], scan_entry->se_bssid[4], scan_entry->se_bssid[5],
                    rssi, ATH_RSSI_OUT(scan_entry->se_avgrssi), 
                    scan_entry->se_phy_mode, wlan_channel_ieee(scan_entry->se_chan), scan_entry_parameters->channel_mismatch,
                    atomic_read(&scan_entry->se_refcount),
                    scan_entry);
            }
                                  
            ieee80211_scan_entry_reset_timestamp(scan_entry);
        }

        return scan_entry;
    }

    if (beacon_frame->capability & IEEE80211_CAPINFO_IBSS) {
        if (! IEEE80211_ADDR_EQ(scan_entry->se_bssid, bssid)) {
            IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
                "%s: Update BSSID: MAC=%02X:%02X:%02X:%02X:%02X:%02X BSSID=%02X:%02X:%02X:%02X:%02X:%02X => %02X:%02X:%02X:%02X:%02X:%02X\n",
                __func__,
                scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
                scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
                scan_entry->se_bssid[0], scan_entry->se_bssid[1], scan_entry->se_bssid[2],
                scan_entry->se_bssid[3], scan_entry->se_bssid[4], scan_entry->se_bssid[5],
                bssid[0], bssid[1], bssid[2],
                bssid[3], bssid[4], bssid[5]);
        }
        
        /*
         * Adhoc station can leave adhoc cell and create a new cell.
         * In this case BSSID can change
         */

        IEEE80211_ADDR_COPY(scan_entry->se_bssid, bssid);
    }

    /*
     * If client is connected to BSSID, update timestamps and beacon interval
     * even if SSID does not match.
     * This covers hidden SSIDs, where we update at least 2 entries (one with 
     * the correct SSID and one with a hidden SSID) based on reception of a 
     * beacon (with hidden SSID).
     */
    if ((! ssid_match) && 
        (! ieee80211_mlme_is_connected(vaphandle->iv_ic, scan_entry))) {
        return scan_entry;
    }

    scan_entry->se_timestamp      = current_time;    
    OS_MEMCPY(&(scan_entry->se_tsf), scan_entry_parameters->tsf, sizeof(scan_entry->se_tsf));
    
    /*
     * Do not update current entry if any port in the system is connected to it
     * and frame's BSSID does not match the BSSID in the scan entry.
     */
    if (ieee80211_mlme_is_connected(vaphandle->iv_ic, scan_entry) &&
        (! IEEE80211_ADDR_EQ(scan_entry->se_bssid, bssid))) {
        IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
             "%s: BSSID mismatch MAC=%02X:%02X:%02X:%02X:%02X:%02X BSSID=%02X:%02X:%02X:%02X:%02X:%02X FrameBSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
             __func__, 
             scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
             scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
             scan_entry->se_bssid[0], scan_entry->se_bssid[1], scan_entry->se_bssid[2],
             scan_entry->se_bssid[3], scan_entry->se_bssid[4], scan_entry->se_bssid[5],
             bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5]);
             
        return scan_entry;
    }     
    
    /*
     * Some scan entry fields can be updated regardless of SSID match.
     */
    scan_entry->se_intval         = scan_entry_parameters->bintval;
    scan_entry->se_phy_mode       = scan_entry_parameters->phy_mode;
    scan_entry->se_fhdwell        = scan_entry_parameters->fhdwell;
    scan_entry->se_fhindex        = scan_entry_parameters->fhindex;

    /*
     * Update channel. This covers the case in which the AP first starts in
     * one channel, then switches to a different channel in the same band. 
     * Another case is when the AP stays on the same channel but changes the
     * Phy Type (e.g. HT20 to HT40).
     * We do not want 2 entries that are identical except for the channel 
     * number.
     * If the bands are different then we do keep distinct entries.
     */
    if (channel_change) {
        scan_entry->se_chan = scan_entry_parameters->chan;
        scan_entry->se_radar_detected_timestamp = 0;
    }

    /*
     * Record rssi data using extended precision LPF filter.
     *
     * If elapsed time since last update to this entry is smaller than a 
     * specified threshold we calculate a running average of the RSSI values, 
     * otherwise we assume the last RSSI is more representive of the signal 
     * strength. 
     * This covers the case in which the user is constantly moving but scans
     * happen only every minute. A running average of the last 8-10 RSSI values
     * would cover 8-10 minutes, leading to a virtually meaningless value.
     *
     * Do not update the RSSI when the current update is based on a beacon 
     * transmitted on an adjacent channel (indicated by flag "channel_mismatch")
     * since the RSSI in this case is much lower than the actual value.
     *
     * Use a separate timestamp to track RSSI updates. Using the entry's 
     * timestamp (se_timestamp) doesn't work because in the case of beacons 
     * received from adjacent channels we update the entry's timestamp but not
     * the RSSI. When we finally receive a beacon on the correct channel, the
     * entry timestamp is fairly recent, and we end time always averaging the 
     * RSSI instead of ignoring values received too long in the past.
     */
    if (! scan_entry_parameters->channel_mismatch) {
        if (CONVERT_SYSTEM_TIME_TO_MS(current_time - scan_entry->se_rssi_timestamp) > AVERAGE_RSSI_TIME_LIMIT) {
            /* Use latest RSSI as the average */
            scan_entry->se_avgrssi = ATH_RSSI_IN(rssi);
        }
        else {
            /* Average w/ previous samples */
            ATH_RSSI_LPF(scan_entry->se_avgrssi, rssi);
        }

        /* Update current rssi and timestamp */
        scan_entry->se_rssi = rssi;
        scan_entry->se_rssi_timestamp = current_time;
    }

    if (wlan_scan_in_progress(vaphandle)) {
        int    avg_rssi;
        
        avg_rssi = ATH_RSSI_OUT(scan_entry->se_avgrssi);
        if (avg_rssi == ATH_RSSI_DUMMY_MARKER) {
            avg_rssi = 0;
        }

        IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
             "%s: MAC=%02X:%02X:%02X:%02X:%02X:%02X ssid=%.*s l=%d BSSID=%02X:%02X:%02X:%02X:%02X:%02X se_timestamp=%lu subtype=%02X ch=%3d rssi=%2d avg_rssi=%2d ssid_match=%d ch_mismatch=%d\n",
             __func__, 
             scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
             scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
             scan_entry->se_ssid[1],
             &(scan_entry->se_ssid[2]),
             scan_entry->se_ssid[1],
             bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5],
             (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(scan_entry->se_timestamp),
             subtype,
             wlan_channel_ieee(scan_entry->se_chan),
             scan_entry_parameters->channel_mismatch ? 0 : rssi,
             avg_rssi,
             ssid_match,
             scan_entry_parameters->channel_mismatch);
    }

    if (! ssid_match) {
        return scan_entry;
    }

    if (wlan_channel_flags(scan_entry->se_chan) != wlan_channel_flags(scan_entry_parameters->chan)) {
        scan_entry->se_chan = scan_entry_parameters->chan;
    }

    /* If Beacon frame, and if scan entry has a Probe response, and if no update scan IE's, then just return */
    if (     (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
          && (scan_entry->se_subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
          && (ieee80211_ic_override_proberesp_ie_is_clear(vaphandle->iv_ic))      /* Check for allowing over-ride */
        )
    { 
        /* do not update IE , just return */
        IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
                                   "%s: Not updating scan IE, received beacon but have Probe response: MAC[" MACSTR "]\n",
                                   __func__,
                                   MAC2STR( (unsigned char*)&scan_entry->se_macaddr[0] ) );
        
        return scan_entry;
    }

    /*
     * Keep track of the WCN IE's (from beacon and probe response each).
     */
    do {
        if (scan_entry->se_subtype == subtype) {
            /* New frame has the same subtype previously */
            break;
        }
        /* Else our stored scan_entry is from a different frame type than this new one */

        ASSERT((scan_entry->se_subtype == IEEE80211_FC0_SUBTYPE_BEACON) || 
               (scan_entry->se_subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP));

        if ((!scan_entry_parameters->ie_list.wcn) && (!scan_entry->se_ie_list.wcn)) {
            /* No WCN IE to take care of */
            scan_entry->se_alt_wcn_ie_len = 0;     /* to indicate no alternate WCN IE */
            break;
        }

        if (!scan_entry->se_alt_wcn_ie) {
            /* allocate this additional buffer for alternate WCN IE */
            scan_entry->se_alt_wcn_ie  = (u_int8_t*) OS_MALLOC(st_osdev, IEEE80211_MAX_IE_LEN + 2, 0);

            scan_entry->se_alt_wcn_ie_len = 0;     /* to indicate no alternate WCN IE */
            if (scan_entry->se_alt_wcn_ie == NULL) {
                /* Out of memory. Ignored this WCN IE */
                break;
            }
        }

        /* Store the old WCN IE. */
        if (!scan_entry->se_ie_list.wcn) {
            /* Existing WCN IE is empty. */
            scan_entry->se_alt_wcn_ie_len = 0;     /* to indicate no alternate WCN IE */
        }
        else {
            int alt_wcn_ie_len = 2 + scan_entry->se_ie_list.wcn[1];
            ASSERT(alt_wcn_ie_len <= (IEEE80211_MAX_IE_LEN + 2));
            OS_MEMCPY(scan_entry->se_alt_wcn_ie, scan_entry->se_ie_list.wcn, alt_wcn_ie_len);
            scan_entry->se_alt_wcn_ie_len = alt_wcn_ie_len;
        }
    } while (false);
    scan_entry->se_subtype = subtype;

    /* 
     * Adjust size of beacon buffer if necessary. The required buffer size is the new beacon
     * frame plus the alternate WCN IE (if any).
     */
    beacon_length_alt = beacon_length + scan_entry->se_alt_wcn_ie_len;
    if ((scan_entry->se_beacon_data == NULL) ||
        (scan_entry->se_beacon_alloc < beacon_length_alt)) {
        u_int8_t    *old_beacon_data = scan_entry->se_beacon_data;

        /* We are allocating a buffer for the new beacon frame plus the alternate WCN IE */
        scan_entry->se_beacon_data  = (u_int8_t*) OS_MALLOC(st_osdev, beacon_length_alt, 0);

        if (scan_entry->se_beacon_data == NULL) {
            /* restore previous IE buffer otherwise all IE pointers would become invalid */
            scan_entry->se_beacon_data = old_beacon_data;
            IEEE80211_SCANENTRY_PRINTF(vaphandle->iv_ic, IEEE80211_MSG_SCANENTRY, 
                "%s: Failed to create beacon buffer: MAC=%02X:%02X:%02X:%02X:%02X:%02X p=%08p\n",
                __func__,
                scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
                scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
                scan_entry);

            return NULL;
        }

        scan_entry->se_beacon_alloc = beacon_length_alt;
        scan_entry->se_beacon_len   = beacon_length;

        if (old_beacon_data) {
            OS_FREE(old_beacon_data);
        }
    }

    /*
     * Fields already initialized:
     *     macaddress, se_beacon_alloc
     */

    /* Copy the new beacon frame and add the WCN IE (if any) later */
    OS_MEMCPY(scan_entry->se_beacon_data, beacon_frame, beacon_length);
    scan_entry->se_beacon_len = beacon_length;  /* just beacon frame; not counting the alternate WCN IE yet */

    scan_entry->se_ie_data = scan_entry->se_beacon_data + offsetof(struct ieee80211_beacon_frame, info_elements);
    scan_entry->se_ie_len  = (u_int16_t) (beacon_length - offsetof(struct ieee80211_beacon_frame, info_elements));

    /*
     * Add the alternate copy of WCN IE to end of IE buffer.
     */
    if (scan_entry->se_alt_wcn_ie_len > 0) {
        OS_MEMCPY((u_int8_t *)scan_entry->se_beacon_data + beacon_length, 
                  scan_entry->se_alt_wcn_ie, 
                  scan_entry->se_alt_wcn_ie_len);
        
        scan_entry->se_beacon_len += scan_entry->se_alt_wcn_ie_len;
        ASSERT(scan_entry->se_beacon_len <= scan_entry->se_beacon_alloc);

        scan_entry->se_ie_len += scan_entry->se_alt_wcn_ie_len;
    }

    IEEE80211_ADDR_COPY(scan_entry->se_bssid, bssid);

    scan_entry->se_capinfo        = scan_entry_parameters->capinfo;
    scan_entry->se_erp            = scan_entry_parameters->erp;
    scan_entry->se_timoff         = scan_entry_parameters->timoff;
    if (scan_entry_parameters->ie_list.tim != NULL) {
        const struct ieee80211_tim_ie    *tim =
            (const struct ieee80211_tim_ie *) scan_entry_parameters->ie_list.tim;

        scan_entry->se_dtimperiod = tim->tim_period;
    }

    /* Use IE offsets calculated from scan parameters to update scan entry */
#define convert_pointer(_address, _base1, _base2) \
    (_address != NULL) ? (((u_int8_t *) (_address) - (u_int8_t *) (_base1)) + (u_int8_t *) (_base2)) : NULL

    scan_entry->se_ie_list = scan_entry_parameters->ie_list;

    /* New info_element needs also be added in ieee80211_parse_beacon */
    scan_entry->se_ie_list.tim         = convert_pointer(scan_entry->se_ie_list.tim,        beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.country     = convert_pointer(scan_entry->se_ie_list.country,    beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.ssid        = convert_pointer(scan_entry->se_ie_list.ssid,       beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.rates       = convert_pointer(scan_entry->se_ie_list.rates,      beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.xrates      = convert_pointer(scan_entry->se_ie_list.xrates,     beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.csa         = convert_pointer(scan_entry->se_ie_list.csa,        beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.xcsa        = convert_pointer(scan_entry->se_ie_list.xcsa,       beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.wpa         = convert_pointer(scan_entry->se_ie_list.wpa,        beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.wcn         = convert_pointer(scan_entry->se_ie_list.wcn,        beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.rsn         = convert_pointer(scan_entry->se_ie_list.rsn,        beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.wps         = convert_pointer(scan_entry->se_ie_list.wps,        beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.wmeinfo     = convert_pointer(scan_entry->se_ie_list.wmeinfo,    beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.wmeparam    = convert_pointer(scan_entry->se_ie_list.wmeparam,   beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.quiet       = convert_pointer(scan_entry->se_ie_list.quiet,      beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.htcap       = convert_pointer(scan_entry->se_ie_list.htcap,      beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.htinfo      = convert_pointer(scan_entry->se_ie_list.htinfo,     beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.athcaps     = convert_pointer(scan_entry->se_ie_list.athcaps,    beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.athextcaps  = convert_pointer(scan_entry->se_ie_list.athextcaps, beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.sfa         = convert_pointer(scan_entry->se_ie_list.sfa,        beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.vendor      = convert_pointer(scan_entry->se_ie_list.vendor,     beacon_frame, scan_entry->se_beacon_data);
    scan_entry->se_ie_list.qbssload    = convert_pointer(scan_entry->se_ie_list.qbssload,   beacon_frame, scan_entry->se_beacon_data);
#if ATH_SUPPORT_WAPI
    scan_entry->se_ie_list.wapi        = convert_pointer(scan_entry->se_ie_list.wapi,       beacon_frame, scan_entry->se_beacon_data);
#endif

    return scan_entry;
#undef AVERAGE_RSSI_TIME_LIMIT
}

/*
 * Process a beacon or probe response frame; create an
 * entry in the scan cache or update any previous entry.
 */
static struct ieee80211_scan_entry *
ieee80211_scan_entry_add(wlan_if_t                            vaphandle, 
                         ieee80211_scan_table_t               scan_table,
                         const struct ieee80211_frame         *wh,
                         u_int32_t                            frame_length,
                         int                                  subtype, 
                         int                                  rssi, 
                         systime_t                            current_time,
                         struct ieee80211_scanentry_params    *scan_entry_parameters)
{
    const u_int8_t                       *macaddr = wh->i_addr2;
    const u_int8_t                       *bssid   = wh->i_addr3;
    struct ieee80211_scan_entry          *scan_entry_match = NULL;
    int                                  hash;
    struct ieee80211_beacon_frame        *beacon_frame = (struct ieee80211_beacon_frame *)&wh[1];
    int                                  beacon_length = frame_length - sizeof(struct ieee80211_frame);
    bool                                 ssid_match = false;

    hash = STA_HASH(macaddr);

    spin_lock(&(scan_table->st_lock));

    {
        /* limit the scope of this variable to avoid confusion with scan_entry_match */ 
        struct ieee80211_scan_entry    *current_scan_entry;

        LIST_FOREACH(current_scan_entry, &(scan_table->st_hash[hash]), se_hash) {
            if (ieee80211_is_scan_entry_match(current_scan_entry, 
                                              macaddr, 
                                              (struct ieee80211_ie_ssid *) (scan_entry_parameters->ie_list.ssid), 
                                              scan_entry_parameters->chan,
                                              &ssid_match)) {
                /*
                 * Save the perfect match (MAC address + SSID)
                 */
                if (ssid_match) {
                    ASSERT(scan_entry_match == NULL);
                    scan_entry_match = current_scan_entry;
                }

                ieee80211_scan_entry_update(vaphandle,
                                            current_scan_entry,
                                            bssid,
                                            ssid_match,
                                            scan_entry_parameters,
                                            beacon_frame,
                                            beacon_length,
                                            subtype, 
                                            rssi, 
                                            current_time,
                                            scan_table->st_osdev);
            }
        }
    }

    /* if no (MAC address + SSID) matches found, create a new entry. */
    if (scan_entry_match == NULL) {
        scan_entry_match = ieee80211_create_new_scan_entry(vaphandle,
                                                           macaddr, 
                                                           scan_entry_parameters->ie_list.ssid,
                                                           scan_entry_parameters->chan,
                                                           subtype,
                                                           scan_table->st_osdev);
        if (scan_entry_match != NULL) {
            /* Update the newly created entry */
            ieee80211_scan_entry_update(vaphandle,
                                        scan_entry_match,
                                        bssid,
                                        true,
                                        scan_entry_parameters,
                                        beacon_frame,
                                        beacon_length,
                                        subtype, 
                                        rssi, 
                                        current_time,
                                        scan_table->st_osdev);

            /* insert new entry in scan table */
            TAILQ_INSERT_TAIL(&(scan_table->st_entry), scan_entry_match, se_list);
            LIST_INSERT_HEAD(&(scan_table->st_hash[hash]), scan_entry_match, se_hash);
        }
        else {
            IEEE80211_SCANENTRY_PRINTF(scan_table->st_ic, IEEE80211_MSG_SCANENTRY, 
                "%s: CANNOT CREATE SCAN ENTRY %02X:%02X:%02X:%02X:%02X:%02X current_time=%lu\n",
                __func__, 
                macaddr[0], macaddr[1], macaddr[2],
                macaddr[3], macaddr[4], macaddr[5],
                (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time));
        }
    }

    spin_unlock(&(scan_table->st_lock));

    /*
     * Notify callback to give opportunity to handle the scan entry.
     */
    IEEE80211_DELIVER_EVENT_STA_SCAN_ENTRY_UPDATE(vaphandle, scan_entry_match);

    return scan_entry_match;
}

#if ATH_SUPPORT_MULTIPLE_SCANS

struct ieee80211_beacon_information {
    const struct ieee80211_frame         *wh;
    u_int32_t                            frame_length;
    int                                  subtype;
    int                                  rssi;
    systime_t                            timestamp;
    struct ieee80211_scan_entry          *scan_entry_match;
    struct ieee80211_scanentry_params    *scan_entry_parameters;
};

static void
ieee80211_vap_iter_add_bss_entry(void *arg, struct ieee80211vap *vaphandle)
{
    struct ieee80211_beacon_information    *beacon_information = arg;
    struct ieee80211_scan_entry            *scan_entry_match;

    if (beacon_information->scan_entry_parameters->channel_mismatch) {
        if (ieee80211_vap_vap_ind_is_set(vaphandle)) {
            /* ignore leaked beacons when ind vap is set */
            return;
        }
    }

    scan_entry_match = ieee80211_scan_entry_add(vaphandle, 
                                                ieee80211_vap_get_scan_table(vaphandle),
                                                beacon_information->wh,
                                                beacon_information->frame_length,
                                                beacon_information->subtype, 
                                                beacon_information->rssi, 
                                                beacon_information->timestamp,
                                                beacon_information->scan_entry_parameters);
    if (scan_entry_match != NULL) {
        beacon_information->scan_entry_match = scan_entry_match;
    }
}

#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

struct ieee80211_scan_entry *
ieee80211_scan_table_update(struct ieee80211vap          *vap,
                            const struct ieee80211_frame *wh,
                            u_int32_t                    frame_length,
                            int                          subtype, 
                            int                          rssi)
{                              
    struct ieee80211com                    *ic = vap->iv_ic;
    const u_int8_t                         *macaddr      = wh->i_addr2;
    struct ieee80211_beacon_frame          *beacon_frame = (struct ieee80211_beacon_frame *)&(wh[1]);
    int                                    beacon_length = frame_length - sizeof(struct ieee80211_frame);
    struct ieee80211_scanentry_params      scan_entry_parameters;
    struct ieee80211_scan_entry            *scan_entry_match = NULL;

    /* Check for invalid/incomplete beacon frame */
    if (beacon_length < 0) {
        return NULL;
    }

    /*
     * Parse and validate the beacon first
     */
    if (ieee80211_parse_beacon(vap,
                               beacon_frame,
                               wh,
                               beacon_length,
                               subtype,
                               &scan_entry_parameters) != EOK) {
        /* XXX TODO: msg + stats */

        IEEE80211_SCANENTRY_PRINTF(ic, IEEE80211_MSG_SCANENTRY, 
            "%s: error parsing beacon MACADDR=%02X:%02X:%02X:%02X:%02X:%02X\n",
            __func__, 
            macaddr[0], macaddr[1], macaddr[2],
            macaddr[3], macaddr[4], macaddr[5]);

        return NULL; 
    }

    if (scan_entry_parameters.chan == NULL) {
        IEEE80211_SCANENTRY_PRINTF(ic, IEEE80211_MSG_SCANENTRY, 
            "%s: error: no channel MACADDR=%02X:%02X:%02X:%02X:%02X:%02X\n",
            __func__, 
            macaddr[0], macaddr[1], macaddr[2],
            macaddr[3], macaddr[4], macaddr[5]);

        return NULL;
    }
    else {
        /* 
         * In 11b/g we may receive beacons/probe responses transmitted on the
         * adjacent channels, so we must validate the transmission channel 
         * based on the active regulatory domain.
         *
         * ieee80211_parse_beacon should set scan_entry_parameters.chan to NULL,
         * but we add the following check to be consistent with previous driver
         * releases.
         */
        if (scan_entry_parameters.channel_mismatch && 
            isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, scan_entry_parameters.chan)))
        {
            return NULL;
        }
    }

#if ATH_SUPPORT_MULTIPLE_SCANS
    /*
     * Multiple-VAP scan supported.
     * Broadcast frames (beacons) are automatically sent to all VAPs from
     * the RX path.
     * Probe responses are directed and thus forwarded only to the destination
     * VAP. In this case we resend the information to all other VAPs.
     *
     * Frames *not* directed to our AP are automatically forwarded to all VAPs,
     * so forward directed frames too so that all VAPs will be notified of the
     * same frames.
     */
    if ((IEEE80211_ADDR_EQ(wh->i_addr3, ieee80211_node_get_bssid(vap->iv_bss)))
        && (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)) {
        struct ieee80211_beacon_information    beacon_information;

        OS_MEMZERO(&(beacon_information), sizeof(beacon_information));
        beacon_information.wh                    = wh;
        beacon_information.frame_length          = frame_length;
        beacon_information.subtype               = subtype;
        beacon_information.rssi                  = rssi;
        beacon_information.timestamp             = OS_GET_TIMESTAMP();
        beacon_information.scan_entry_match      = NULL;
        beacon_information.scan_entry_parameters = &scan_entry_parameters;

        wlan_iterate_vap_list(ic, ieee80211_vap_iter_add_bss_entry, (void *) &beacon_information);
        
        scan_entry_match = beacon_information.scan_entry_match;
    }
    else {
        scan_entry_match = ieee80211_scan_entry_add(vap, 
                                                    ieee80211_vap_get_scan_table(vap),
                                                    wh,
                                                    frame_length,
                                                    subtype, 
                                                    rssi, 
                                                    OS_GET_TIMESTAMP(),
                                                    &scan_entry_parameters);
    }
    
#else
    scan_entry_match = ieee80211_scan_entry_add(vap, 
                                                ieee80211_vap_get_scan_table(vap),
                                                wh,
                                                frame_length,
                                                subtype, 
                                                rssi, 
                                                OS_GET_TIMESTAMP(),
                                                &scan_entry_parameters);
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

    return scan_entry_match;
}

static void
ieee80211_release_entry(struct ieee80211com         *st_ic,
                        struct ieee80211_scan_entry *scan_entry)
{
    ASSERT(atomic_read(&scan_entry->se_refcount) == 0);

    if (atomic_read(&scan_entry->se_refcount) == 0) {
        /*
         * All IE-related pointers inside the scan entry point to the scan entry
         * itself. No extra memory was allocated for them, so there's no need to 
         * free anything.
         */
        if (scan_entry->se_beacon_data) {
            OS_FREE(scan_entry->se_beacon_data);
        }
        if (scan_entry->se_alt_wcn_ie) {
            scan_entry->se_alt_wcn_ie_len = 0;
            OS_FREE(scan_entry->se_alt_wcn_ie);
        }
        /* here dump the trace info if enabled*/
        IEEE80211_DUMP_TRACESE(scan_entry);
        OS_FREE(scan_entry);
    }
    else {
        IEEE80211_SCANENTRY_PRINTF(st_ic, IEEE80211_MSG_SCANENTRY, 
            "%s: CANNOT RELEASE SCAN ENTRY %02X:%02X:%02X:%02X:%02X:%02X refcount=%d p=%08p\n",
            __func__, 
            scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
            scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
            atomic_read(&scan_entry->se_refcount),
            scan_entry);
    }
}

int ieee80211_scan_table_iterate(ieee80211_scan_table_t scan_table, ieee80211_scan_iter_func shandler, void *arg) 
{
    struct ieee80211_scan_entry    *scan_entry;
    int                            status = 0;

    spin_lock(&(scan_table->st_lock));

    TAILQ_FOREACH(scan_entry, &(scan_table->st_entry), se_list) {
        status = (*shandler)(arg, scan_entry);
        if (status != EOK) {
            break;
        }
    }

    spin_unlock(&(scan_table->st_lock));

    return status;
}

/*
 * Flush all entries in the scan cache.
 * Only remove entries when reference count reaches 0.
 * This will prevent removal of the scan entry corresponding to the AP to
 * which we are connected.
 */
void
ieee80211_scan_table_flush(ieee80211_scan_table_t scan_table)
{
    struct ieee80211_scan_entry    *scan_entry, *next;

    spin_lock(&(scan_table->st_lock));
    TAILQ_FOREACH_SAFE(scan_entry, &(scan_table->st_entry), se_list, next) {
        IEEE80211_SCANENTRY_PRINTF(scan_table->st_ic, IEEE80211_MSG_SCANENTRY, 
            "%s: %02X:%02X:%02X:%02X:%02X:%02X\n",
            __func__, 
            scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
            scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5]);

        if (ieee80211_scan_entry_remove_reference(scan_entry) == 0) {
            /*
             * If last reference removed, release entry.
             */
            TAILQ_REMOVE(&(scan_table->st_entry), scan_entry, se_list);
            LIST_REMOVE(scan_entry, se_hash);

            ieee80211_release_entry(scan_table->st_ic, scan_entry);
        }
        else {
            IEEE80211_SCANENTRY_PRINTF(scan_table->st_ic, IEEE80211_MSG_SCANENTRY, 
                "%s: %02X:%02X:%02X:%02X:%02X:%02X cannot flush entry refcount=%d p=%8p\n",
                __func__, 
                scan_entry->se_macaddr[0], scan_entry->se_macaddr[1], scan_entry->se_macaddr[2],
                scan_entry->se_macaddr[3], scan_entry->se_macaddr[4], scan_entry->se_macaddr[5],
                atomic_read(&scan_entry->se_refcount),
                scan_entry);

            /*
             * There are other references to this scan entry, put it back in
             * the list
             */
            ieee80211_scan_entry_add_reference(scan_entry);
        }
    }
    spin_unlock(&(scan_table->st_lock));
}

static int
ieee80211_scan_table_init(struct ieee80211com    *ic,
                          ieee80211_scan_table_t *st, 
                          osdev_t                osdev)
{
    int    i;

    if (*st) 
        return EINPROGRESS; /* already attached ? */

    *st = (ieee80211_scan_table_t) OS_MALLOC(osdev, (sizeof(struct ieee80211_scan_table)), 0);

    if (*st) {
        OS_MEMZERO(*st, sizeof(struct ieee80211_scan_table));

        (*st)->st_ic    = ic; 
        (*st)->st_osdev = osdev; 

        TAILQ_INIT(&((*st)->st_entry));

        /* XXX lock alloc failure ?*/
        spin_lock_init(&(*st)->st_lock);
        TAILQ_INIT(&((*st)->st_entry));
        for (i = 0;i < STA_HASHSIZE; ++i) {
            LIST_INIT(&(*st)->st_hash[i]);
        }

        return EOK;
    }

    return ENOMEM;
}

static int
ieee80211_scan_table_terminate(ieee80211_scan_table_t *st)
{
    int    i;

    if (*st == NULL) 
        return EINPROGRESS; /* already detached ? */

    ieee80211_scan_table_flush(*st);

    TAILQ_INIT(&(*st)->st_entry);
    for (i = 0; i < STA_HASHSIZE; ++i) {
        LIST_INIT(&(*st)->st_hash[i]);
    }
    spin_lock_destroy(&(*st)->st_lock);

    OS_FREE(*st);

    *st = NULL;

    return EOK;
}

/*
 * If multiple-VAP scan is enabled, each VAP holds a scan table which is
 * created during VAP initialization. In this case, the IC tables is not
 * used.
 *
 * If multiple-VAP scan is disabled, the scan table is kept by the IC and
 * shared by VAPs. The VAP scan tables are not used.
 */
#if ATH_SUPPORT_MULTIPLE_SCANS

int
ieee80211_scan_table_attach(struct ieee80211com    *ic,
                            ieee80211_scan_table_t *st, 
                            osdev_t                osdev)
{
    /* IC scan table not used */
    *st = NULL;

    /* Function returns EOK to indicate all necessary initialization has been done. */
    return EOK;
}

int
ieee80211_scan_table_detach(ieee80211_scan_table_t *st)
{
    /* IC scan table not used */
    *st = NULL;

    /* Function returns EOK to indicate all necessary initialization has been done. */
    return EOK;
}

int
ieee80211_scan_table_vattach(wlan_if_t              vaphandle,
                             ieee80211_scan_table_t *st, 
                             osdev_t                osdev)
{
    return ieee80211_scan_table_init(vaphandle->iv_ic, st, osdev);
}

int
ieee80211_scan_table_vdetach(ieee80211_scan_table_t *st)
{
    return ieee80211_scan_table_terminate(st);
}

#else    /* ATH_SUPPORT_MULTIPLE_SCANS */

int
ieee80211_scan_table_attach(struct ieee80211com    *ic,
                            ieee80211_scan_table_t *st, 
                            osdev_t                osdev)
{
    return ieee80211_scan_table_init(ic, st, osdev);
}

int
ieee80211_scan_table_detach(ieee80211_scan_table_t *st)
{
    return ieee80211_scan_table_terminate(st);
}

int
ieee80211_scan_table_vattach(wlan_if_t              vaphandle,
                             ieee80211_scan_table_t *st, 
                             osdev_t                osdev)
{
    /* VAP scan table not used */
    *st = NULL;

    /* Function returns EOK to indicate all necessary initialization has been done. */
    return EOK;
}

int
ieee80211_scan_table_vdetach(ieee80211_scan_table_t *st)
{
    /* VAP scan table not used */
    *st = NULL;

    /* Function returns EOK to indicate all necessary initialization has been done. */
    return EOK;
}

#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

int wlan_scan_table_iterate(wlan_if_t vaphandle, ieee80211_scan_iter_func shandler, void *arg)
{
    return ieee80211_scan_table_iterate(ieee80211_vap_get_scan_table(vaphandle), shandler, arg);
}

void wlan_scan_entry_lock(wlan_scan_entry_t scan_entry)
{
    ieee80211_scan_entry_lock(scan_entry);
}

void wlan_scan_entry_unlock(wlan_scan_entry_t scan_entry)
{
    ieee80211_scan_entry_unlock(scan_entry);
}

u_int32_t wlan_scan_entry_phymode(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_phymode(scan_entry);
}

u_int8_t *wlan_scan_entry_ssid(wlan_scan_entry_t scan_entry, u_int8_t *len)
{
    if (len == NULL)
        return NULL;

    return ieee80211_scan_entry_ssid(scan_entry, len);
}

u_int8_t wlan_scan_entry_dtimperiod(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_dtimperiod(scan_entry);
}

u_int8_t *wlan_scan_entry_tim(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_tim(scan_entry);
}

u_int8_t *wlan_scan_entry_macaddr(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_macaddr(scan_entry);
}

u_int8_t *wlan_scan_entry_bssid(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_bssid(scan_entry);
}

u_int16_t wlan_scan_entry_capinfo(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_capinfo(scan_entry);
}

u_int16_t wlan_scan_entry_beacon_interval(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_beacon_interval(scan_entry);
}

void wlan_scan_entry_tsf(wlan_scan_entry_t scan_entry, u_int8_t *tsf)
{
    OS_MEMCPY(tsf, ieee80211_scan_entry_tsf(scan_entry), 8);
}

void wlan_scan_entry_reset_timestamp(wlan_scan_entry_t scan_entry)
{
    ieee80211_scan_entry_reset_timestamp(scan_entry);
}

u_int8_t wlan_scan_entry_rssi(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_rssi(scan_entry);
}

int wlan_scan_entry_copy_beacon_data(wlan_scan_entry_t scan_entry, u_int8_t *beacon_buf, u_int16_t *beacon_len)
{
    u_int8_t     *internal_buffer;
    u_int16_t    internal_buffer_length;

    /* beacon_buf can be NULL, beacon_len must be a valid pointer. */
    ASSERT(beacon_len != NULL);

    internal_buffer = ieee80211_scan_entry_beacon_data(scan_entry, &internal_buffer_length);
    /* 
     * If caller passed a buffer where to copy the IE, check the length to 
     * make sure it's large enough.
     * If no buffer is passed, just return the length of the IE blob.
     */
    if (beacon_buf != NULL)
    {
        if (*beacon_len >= internal_buffer_length)
        {
            OS_MEMCPY(beacon_buf, internal_buffer, internal_buffer_length);
            *beacon_len = internal_buffer_length;

            return EOK;
        }
    }
    else
    {
        *beacon_len = internal_buffer_length;

        return EOK;
    }

    return EINVAL;
}

int wlan_scan_entry_beacon_len(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_beacon_len(scan_entry);
}

int wlan_scan_entry_copy_ie_data(wlan_scan_entry_t scan_entry, u_int8_t *iebuf, u_int16_t *ie_len)
{
    u_int8_t     *internal_buffer;
    u_int16_t    internal_buffer_length;

    /* iebuf can be NULL, ie_len must be a valid pointer. */
    ASSERT(ie_len != NULL);

    internal_buffer = ieee80211_scan_entry_ie_data(scan_entry, &internal_buffer_length);
    /* 
     * If caller passed a buffer where to copy the IE, check the length to 
     * make sure it's large enough.
     * If no buffer is passed, just return the length of the IE blob.
     */
    if (iebuf != NULL)
    {
        if (*ie_len >= internal_buffer_length)
        {
            OS_MEMCPY(iebuf, internal_buffer, internal_buffer_length);
            *ie_len = internal_buffer_length;

            return EOK;
        }
    }
    else
    {
        *ie_len = internal_buffer_length;

        return EOK;
    }

    *ie_len = 0;
    return EINVAL;
}

int wlan_scan_entry_ie_len(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_ie_len(scan_entry);
}

wlan_chan_t wlan_scan_entry_channel(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_channel(scan_entry);
}

u_int8_t wlan_scan_entry_erpinfo(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_erpinfo(scan_entry);
}

u_int8_t *wlan_scan_entry_rates(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_rates(scan_entry);
}

u_int8_t *wlan_scan_entry_xrates(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_xrates(scan_entry);
}

int wlan_scan_entry_rsncaps(wlan_if_t vaphandle, wlan_scan_entry_t scan_entry, u_int16_t *rsncaps)
{
    u_int8_t                     *rsn_ie;
    struct ieee80211_rsnparms    rsn_parms;

    rsn_ie = ieee80211_scan_entry_rsn(scan_entry);
    if (rsn_ie == NULL)
        return -EIO;

    if (ieee80211_parse_rsn(vaphandle, rsn_ie, &rsn_parms) != IEEE80211_STATUS_SUCCESS)
        return -EIO;

    *rsncaps = rsn_parms.rsn_caps;
    return EOK;
}

u_int8_t *wlan_scan_entry_rsn(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_rsn(scan_entry);
}

#if ATH_SUPPORT_WAPI
u_int8_t *wlan_scan_entry_wapi(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_wapi(scan_entry);
}
#endif

u_int8_t *wlan_scan_entry_wpa(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_wpa(scan_entry);
}

u_int8_t *wlan_scan_entry_wps(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_wps(scan_entry);
}

u_int8_t *wlan_scan_entry_sfa(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_sfa(scan_entry);
}

u_int8_t *wlan_scan_entry_channelswitch(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_csa(scan_entry);
}

u_int8_t *wlan_scan_entry_extendedchannelswitch(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_xcsa(scan_entry);
}

u_int8_t *wlan_scan_entry_htinfo(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_htinfo(scan_entry);
}

u_int8_t *wlan_scan_entry_htcap(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_htcap(scan_entry);
}

u_int8_t *wlan_scan_entry_quiet(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_quiet(scan_entry);
}

u_int8_t *wlan_scan_entry_qbssload(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_qbssload(scan_entry);
}

u_int8_t *wlan_scan_entry_vendor(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_vendor(scan_entry);
}

int wlan_scan_entry_country(wlan_scan_entry_t scan_entry, u_int8_t *country)
{
    struct ieee80211_country_ie    *country_ie;

    if (country == NULL)
        return EINVAL;

    country_ie = (struct ieee80211_country_ie *) ieee80211_scan_entry_country(scan_entry);

    if (country_ie == NULL)
        return ENOMEM;

    OS_MEMCPY(country, country_ie->cc, 3);
    country[3] = 0; /* terminate with NULL */

    return EOK;
}

u_int8_t *wlan_scan_entry_wmeinfo_ie(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_wmeinfo_ie(scan_entry);
}

u_int8_t *wlan_scan_entry_wmeparam_ie(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_wmeparam_ie(scan_entry);
}

u_int32_t wlan_scan_entry_age(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_age(scan_entry);
}

u_int32_t wlan_scan_entry_status(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_status(scan_entry);
}

void wlan_scan_entry_set_status(wlan_scan_entry_t scan_entry, u_int32_t status)
{
    ieee80211_scan_entry_set_status(scan_entry, status);
}

u_int32_t wlan_scan_entry_assoc_state(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_assoc_state(scan_entry);
}

void wlan_scan_entry_set_assoc_state(wlan_scan_entry_t scan_entry, u_int32_t state)
{
    ieee80211_scan_entry_set_assoc_state(scan_entry, state);
}

u_int8_t wlan_scan_entry_reference_count(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_reference_count(scan_entry);
}

u_int8_t wlan_scan_entry_add_reference_dbg(
        wlan_scan_entry_t scan_entry, const char *func, int line)
{
    return ieee80211_scan_entry_add_reference_dbg(scan_entry, func, line);
}

u_int8_t wlan_scan_entry_remove_reference_dbg(
        wlan_scan_entry_t scan_entry, const char *func, int line)
{
    return ieee80211_scan_entry_remove_reference_dbg(scan_entry, func, line);
}

systime_t wlan_scan_entry_bad_ap_time(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_bad_ap_time(scan_entry);
}

void wlan_scan_entry_set_bad_ap_time(wlan_scan_entry_t scan_entry, systime_t timestamp)
{
    ieee80211_scan_entry_set_bad_ap_time(scan_entry, timestamp);
}

u_int32_t wlan_scan_entry_assoc_cost(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_assoc_cost(scan_entry);
}

systime_t wlan_scan_entry_lastassoc(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_lastassoc(scan_entry);
}

void wlan_scan_entry_set_lastassoc(wlan_scan_entry_t scan_entry, systime_t timestamp)
{
    ieee80211_scan_entry_set_lastassoc(scan_entry, timestamp);
}

systime_t wlan_scan_entry_lastdeauth(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_lastdeauth(scan_entry);
}

void wlan_scan_entry_set_lastdeauth(wlan_scan_entry_t scan_entry, systime_t timestamp)
{
    ieee80211_scan_entry_set_lastdeauth(scan_entry, timestamp);
}

void wlan_scan_entry_set_assoc_cost(wlan_scan_entry_t scan_entry, u_int32_t cost)
{
    ieee80211_scan_entry_set_assoc_cost(scan_entry, cost);
}

void wlan_scan_entry_set_demerit_utility(wlan_scan_entry_t scan_entry, bool enable)
{
    ieee80211_scan_entry_set_demerit_utility(scan_entry, enable);
}

enum ieee80211_opmode wlan_scan_entry_bss_type(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_bss_type(scan_entry);
}

u_int8_t wlan_scan_entry_privacy(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_privacy(scan_entry);
}

u_int8_t *wlan_scan_entry_athcaps(wlan_scan_entry_t scan_entry)
{
    return (u_int8_t *) ieee80211_scan_entry_athcaps(scan_entry);
}

u_int32_t wlan_scan_entry_utility(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_utility(scan_entry);
}

u_int32_t wlan_scan_entry_chanload(wlan_scan_entry_t scan_entry)
{
    return ieee80211_scan_entry_chanload(scan_entry);
}

void wlan_scan_table_flush(wlan_if_t vaphandle)
{
    ieee80211_scan_table_flush(ieee80211_vap_get_scan_table(vaphandle));
}

#endif
