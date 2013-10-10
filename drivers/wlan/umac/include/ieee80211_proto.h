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

#ifndef _ATH_STA_IEEE80211_PROTO_H
#define _ATH_STA_IEEE80211_PROTO_H

/*
 * 802.11 protocol implementation definitions.
 */

enum ieee80211_state {
    IEEE80211_S_INIT        = 0,    /* default state */
    IEEE80211_S_SCAN        = 1,    /* scanning */
    IEEE80211_S_JOIN        = 2,    /* join */
    IEEE80211_S_AUTH        = 3,    /* try to authenticate */
    IEEE80211_S_ASSOC       = 4,    /* try to assoc */
    IEEE80211_S_RUN         = 5,    /* associated */
    IEEE80211_S_DFS_WAIT,
    IEEE80211_S_WAIT_TXDONE,    /* waiting for pending tx before going to INI */
    IEEE80211_S_STOPPING,
    IEEE80211_S_STANDBY,        /* standby, waiting to re-start */

    IEEE80211_S_MAX /* must be last */
};

enum ieee80211_state_event {
    IEEE80211_STATE_EVENT_DOWN    = 0, 
    IEEE80211_STATE_EVENT_JOIN    , 
    IEEE80211_STATE_EVENT_SCAN_START    , 
    IEEE80211_STATE_EVENT_SCAN_END    , 
    IEEE80211_STATE_EVENT_TXDONE  ,
    IEEE80211_STATE_EVENT_UP      ,
    IEEE80211_STATE_EVENT_DFS_WAIT   ,
    IEEE80211_STATE_EVENT_DFS_CLEAR  ,
    IEEE80211_STATE_EVENT_FORCE_STOP  , 
    IEEE80211_STATE_EVENT_STANDBY,
    IEEE80211_STATE_EVENT_RESUME,
    IEEE80211_STATE_EVENT_BSS_NODE_FREED,
    IEEE80211_STATE_EVENT_CHAN_SET   ,
};


#define IEEE80211_ACTION_BUF_SIZE 256

struct ieee80211_action_mgt_args {
    u_int8_t    category;
    u_int8_t    action;
    u_int32_t   arg1;
    u_int32_t   arg2;
    u_int32_t   arg3;
    u_int8_t    *arg4;
};

struct ieee80211_action_mgt_buf {
    u_int8_t    buf[IEEE80211_ACTION_BUF_SIZE];
};

typedef struct  _ieee80211_vap_state_info {
    u_int32_t     iv_state;
    spinlock_t    iv_state_lock; /* lock to serialize access to vap state machine */
    bool          iv_sm_running; /* indicates that the VAP SM is running */ 
}ieee80211_vap_state_info;

extern const char *ieee80211_mgt_subtype_name[];
extern const char *ieee80211_wme_acnames[];
/*
 * flags for ieee80211_send_cts
 */
#define IEEE80211_CTS_SMPS 1

int ieee80211_state_event(struct ieee80211vap *vap, enum ieee80211_state_event event);

void ieee80211_proto_attach(struct ieee80211com *ic);
void ieee80211_proto_detach(struct ieee80211com *ic);

void ieee80211_proto_vattach(struct ieee80211vap *vap);
void ieee80211_proto_vdetach(struct ieee80211vap *vap);

int  ieee80211_vap_start(struct ieee80211vap *vap);
int  ieee80211_vap_join(struct ieee80211vap *vap);
void ieee80211_vap_stop(struct ieee80211vap *vap, bool force);
void ieee80211_vap_standby(struct ieee80211vap *vap);
void ieee80211_vap_bss_node_freed(struct ieee80211vap *vap);

int ieee80211_parse_wmeparams(struct ieee80211vap *vap, u_int8_t *frm, u_int8_t *qosinfo, int forced_update);
int ieee80211_parse_wmeinfo(struct ieee80211vap *vap, u_int8_t *frm, u_int8_t *qosinfo);
int ieee80211_parse_wmeie(u_int8_t *frm, const struct ieee80211_frame *wh, struct ieee80211_node *ni);
int ieee80211_parse_tspecparams(struct ieee80211vap *vap, u_int8_t *frm);
u_int32_t ieee80211_parse_mpdudensity(u_int32_t mpdudensity);
void ieee80211_parse_htcap(struct ieee80211_node *ni, u_int8_t *ie);
void ieee80211_parse_htinfo(struct ieee80211_node *ni, u_int8_t *ie);
int ieee80211_parse_dothparams(struct ieee80211vap *vap, u_int8_t *frm);

int ieee80211_parse_wpa(struct ieee80211vap *vap, u_int8_t *frm, struct ieee80211_rsnparms *rsn);
int ieee80211_parse_rsn(struct ieee80211vap *vap, u_int8_t *frm, struct ieee80211_rsnparms *rsn);

void ieee80211_process_athextcap_ie(struct ieee80211_node *ni, u_int8_t *ie);
void ieee80211_parse_athParams(struct ieee80211_node *ni, u_int8_t *ie);

int ieee80211_process_asresp_elements(struct ieee80211_node *ni,
                                  u_int8_t *frm,
                                  u_int32_t ie_len);

void ieee80211_prepare_qosnulldata(struct ieee80211_node *ni, wbuf_t wbuf, int ac);

int ieee80211_send_qosnulldata(struct ieee80211_node *ni, int ac, int pwr_save);

int ieee80211_send_nulldata(struct ieee80211_node *ni, int pwr_save);

int ieee80211_send_pspoll(struct ieee80211_node *ni);

int ieee80211_send_cts(struct ieee80211_node *ni, int flags);

int ieee80211_send_probereq(struct ieee80211_node *ni,
                            const u_int8_t        *sa,
                            const u_int8_t        *da,
                            const u_int8_t        *bssid,
                            const u_int8_t        *ssid, 
                            const u_int32_t       ssidlen,
                            const void            *optie, 
                            const size_t          optielen);

int ieee80211_send_proberesp(struct ieee80211_node *ni, u_int8_t *macaddr,
                            const void            *optie, 
                            const size_t          optielen);

int ieee80211_send_auth( struct ieee80211_node *ni, u_int16_t seq,
                         u_int16_t status, u_int8_t *challenge_txt, 
                         u_int8_t challenge_len );
int ieee80211_send_deauth(struct ieee80211_node *ni, u_int16_t reason);
int ieee80211_send_disassoc(struct ieee80211_node *ni, u_int16_t reason);
int ieee80211_send_assoc(struct ieee80211_node *ni,
                         int reassoc, u_int8_t *prev_bssid);
int ieee80211_send_assocresp(struct ieee80211_node *ni, 
                             u_int8_t reassoc, u_int16_t reason);
wbuf_t ieee80211_setup_assocresp(struct ieee80211_node *ni, wbuf_t wbuf,
                                 u_int8_t reassoc, u_int16_t reason);
int ieee80211_send_mgmt(struct ieee80211vap *vap,struct ieee80211_node *ni, wbuf_t wbuf, bool force_send);

int ieee80211_send_action( struct ieee80211_node *ni,
                           struct ieee80211_action_mgt_args *actionargs,
                           struct ieee80211_action_mgt_buf  *actionbuf );
#ifdef ATH_SUPPORT_TxBF
int ieee80211_send_v_cv_action(struct ieee80211_node *ni, u_int8_t *data_buf, u_int16_t buf_len);
#endif

int ieee80211_send_bar(struct ieee80211_node *ni, u_int8_t tidno, u_int16_t seqno);

int ieee80211_recv_mgmt(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                        struct ieee80211_rx_status *rs);
int ieee80211_recv_ctrl(struct ieee80211_node *ni, wbuf_t wbuf,
						int subtype, struct ieee80211_rx_status *rs);

ieee80211_scan_entry_t ieee80211_update_beacon(struct ieee80211_node *ni, wbuf_t wbuf,
                                               struct ieee80211_frame *wh, int subtype,
                                               struct ieee80211_rx_status *rs);

void ieee80211_reset_erp(struct ieee80211com *,
                         enum ieee80211_phymode,
                         enum ieee80211_opmode);
void ieee80211_set_shortslottime(struct ieee80211com *, int onoff);


void ieee80211_dump_pkt(struct ieee80211com *ic,
                   const u_int8_t *buf, int len, int rate, int rssi);
void    ieee80211_change_cw(struct ieee80211com *ic);

#if UMAC_SUPPORT_AP || UMAC_SUPPORT_IBSS || UMAC_SUPPORT_BTAMP
struct ieee80211_beacon_offsets;
int
ieee80211_beacon_update(struct ieee80211_node *ni,
                        struct ieee80211_beacon_offsets *bo, wbuf_t wbuf, int mcast);
wbuf_t ieee80211_beacon_alloc(struct ieee80211_node *ni, struct ieee80211_beacon_offsets *bo);

#else
#define ieee80211_beacon_update(ni,bo,wbuf,mcast) (-1)
#define ieee80211_beacon_alloc(ni,bo)  (NULL)

#endif

#if UMAC_SUPPORT_STA
void ieee80211_beacon_miss(struct ieee80211com *);
#else
#define ieee80211_beacon_miss(ic) /**/
#endif
/*
 * Return the size of the 802.11 header for a management or data frame.
 */
INLINE static int
ieee80211_hdrsize(const void *data)
{
    const struct ieee80211_frame *wh = (const struct ieee80211_frame *)data;
    int size = sizeof(struct ieee80211_frame);

    /* NB: we don't handle control frames */
    KASSERT((wh->i_fc[0]&IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL,
            ("%s: control frame", __func__));
    if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
        size += IEEE80211_ADDR_LEN;
    
    if (IEEE80211_QOS_HAS_SEQ(wh)){
        size += sizeof(u_int16_t);
#ifdef ATH_SUPPORT_TxBF
        /* Qos frame with Order bit set indicates an HTC frame */
        if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
            size += sizeof(struct ieee80211_htc);
        }
#endif
    }
    return size;
}

/*
 * Like ieee80211_hdrsize, but handles any type of frame.
 */
static INLINE int
ieee80211_anyhdrsize(const void *data)
{
    const struct ieee80211_frame *wh = (const struct ieee80211_frame *)data;

    if ((wh->i_fc[0]&IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL) {
        switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
            case IEEE80211_FC0_SUBTYPE_CTS:
            case IEEE80211_FC0_SUBTYPE_ACK:
                return sizeof(struct ieee80211_frame_ack);
        }
        return sizeof(struct ieee80211_frame_min);
    } else
        return ieee80211_hdrsize(data);
}

struct wmeParams {
    u_int8_t    wmep_acm;           /* ACM parameter */
    u_int8_t    wmep_aifsn;         /* AIFSN parameters */
    u_int8_t    wmep_logcwmin;      /* cwmin in exponential form */
    u_int8_t    wmep_logcwmax;      /* cwmax in exponential form */
    u_int16_t   wmep_txopLimit;     /* txopLimit */
    u_int8_t    wmep_noackPolicy;   /* No-Ack Policy: 0=ack, 1=no-ack */
};

#define IEEE80211_EXPONENT_TO_VALUE(_exp)  (1 << (u_int32_t)(_exp)) - 1
#define IEEE80211_TXOP_TO_US(_txop)  (u_int32_t)(_txop) << 5
#define IEEE80211_US_TO_TXOP(_us)  (u_int16_t)((u_int32_t)(_us)) >> 5

struct chanAccParams{
    /* XXX: is there any reason to have multiple instances of cap_info??? */
    u_int8_t            cap_info;                   /* U-APSD flag + ver. of the current param set */
    struct wmeParams    cap_wmeParams[WME_NUM_AC];  /* WME params for each access class */ 
};

struct ieee80211_wme_state {
    u_int32_t   wme_flags;
#define	WME_F_AGGRMODE  0x00000001              /* STATUS: WME agressive mode */

    u_int       wme_hipri_traffic;              /* VI/VO frames in beacon interval */
    u_int       wme_hipri_switch_thresh;        /* agressive mode switch thresh */
    u_int       wme_hipri_switch_hysteresis;    /* agressive mode switch hysteresis */

    struct chanAccParams    wme_wmeChanParams;  /* configured WME parameters applied to itself */
    struct chanAccParams    wme_wmeBssChanParams;   /* configured WME parameters broadcasted to STAs */
    struct chanAccParams    wme_chanParams;     /* channel parameters applied to itself */
    struct chanAccParams    wme_bssChanParams;  /* channel parameters broadcasted to STAs */
    u_int8_t                wme_nonAggressiveMode;  /* don't use aggressive params and use WME params */

    /* update hardware tx params after wme state change */
    int	(*wme_update)(struct ieee80211com *);
};

void ieee80211_wme_initparams(struct ieee80211vap *);
void ieee80211_wme_initparams_locked(struct ieee80211vap *);
void ieee80211_wme_updateparams(struct ieee80211vap *);
void ieee80211_wme_updateinfo(struct ieee80211vap *);
void ieee80211_wme_updateparams_locked(struct ieee80211vap *);
void ieee80211_wme_updateinfo_locked(struct ieee80211vap *);
void ieee80211_wme_amp_overloadparams_locked(struct ieee80211com *ic);

/*
 * Beacon frames constructed by ieee80211_beacon_alloc
 * have the following structure filled in so drivers
 * can update the frame later w/ minimal overhead.
 */
struct ieee80211_beacon_offsets {
    u_int16_t	*bo_caps;		/* capabilities */
    u_int8_t	*bo_tim;		/* start of atim/dtim */
    u_int8_t	*bo_wme;		/* start of WME parameters */
    u_int8_t	*bo_tim_trailer;	/* start of fixed-size tim trailer */
    u_int16_t	bo_tim_len;		/* atim/dtim length in bytes */
    u_int16_t	bo_tim_trailerlen;	/* trailer length in bytes */
    u_int8_t	*bo_chanswitch;		/* where channel switch IE will go */
    u_int16_t	bo_chanswitch_trailerlen;
    u_int8_t	*bo_ath_caps;		/* where ath caps is */
    u_int8_t	*bo_xr;			/* start of xr element */
    u_int8_t	*bo_erp;		/* start of ERP element */
    u_int8_t	*bo_htinfo;		/* start of HT Info element */
    u_int8_t    *bo_htinfo_pre_ana; /* start of pre ana HT Info element */ 
    u_int8_t    *bo_htinfo_vendor_specific;/* start of vendor specific HT Info element */ 
    u_int8_t    *bo_appie_buf;  /* start of APP IE buf */
    u_int16_t   bo_appie_buf_len;
    u_int8_t    *bo_obss_scan;          /* start of overlap BSS Scan element */
    u_int8_t    *bo_extcap;
    u_int8_t    *bo_htcap;
#ifdef E_CSA
    u_int8_t    *bo_extchanswitch;  /* start of extended channel switch */
    u_int16_t	bo_extchanswitch_trailerlen;
#endif
#if ATH_SUPPORT_IBSS_DFS
    u_int8_t    *bo_ibssdfs;  /* start of ibssdfs */
    u_int16_t	bo_ibssdfs_trailerlen;    
#endif /* ATH_SUPPORT_IBSS_DFS */
};

/* XXX exposed 'cuz of beacon code botch */
u_int8_t *ieee80211_add_rates(u_int8_t *, const struct ieee80211_rateset *);
u_int8_t *ieee80211_add_xrates(u_int8_t *, const struct ieee80211_rateset *);
u_int8_t *ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len);
u_int8_t *ieee80211_add_erp(u_int8_t *, struct ieee80211com *);
u_int8_t *ieee80211_add_athAdvCap(u_int8_t *, u_int8_t, u_int16_t);
u_int8_t *ieee80211_add_athextcap(u_int8_t *, u_int16_t, u_int8_t);
u_int8_t *ieee80211_add_wmeinfo(u_int8_t *frm, struct ieee80211_node *ni, 
                                u_int8_t wme_subtype, u_int8_t *wme_info, u_int8_t info_len);
u_int8_t *ieee80211_add_wme_param(u_int8_t *, struct ieee80211_wme_state *,
								  int uapsd_enable);
u_int8_t *ieee80211_add_country(u_int8_t *, struct ieee80211vap *vap);
u_int8_t *ieee80211_add_doth(u_int8_t *frm, struct ieee80211vap *vap);
u_int8_t *ieee80211_add_htcap(u_int8_t *, struct ieee80211_node *, u_int8_t);
u_int8_t *ieee80211_add_htcap_pre_ana(u_int8_t *, struct ieee80211_node *, u_int8_t);
u_int8_t *ieee80211_add_htcap_vendor_specific(u_int8_t *, struct ieee80211_node *, u_int8_t);
u_int8_t *ieee80211_add_htinfo(u_int8_t *, struct ieee80211_node *);
u_int8_t *ieee80211_add_htinfo_pre_ana(u_int8_t *, struct ieee80211_node *);
u_int8_t *ieee80211_add_htinfo_vendor_specific(u_int8_t *, struct ieee80211_node *);
void ieee80211_update_htinfo_cmn(struct ieee80211_ie_htinfo_cmn *ie, struct ieee80211_node *ni);
void ieee80211_update_obss_scan(struct ieee80211_ie_obss_scan *, struct ieee80211_node *);
u_int8_t *ieee80211_add_obss_scan(u_int8_t *, struct ieee80211_node *);
u_int8_t *ieee80211_add_extcap(u_int8_t *, struct ieee80211_node *);

u_int8_t *ieee80211_setup_rsn_ie(struct ieee80211vap *vap, u_int8_t *ie);
u_int8_t *ieee80211_setup_wpa_ie(struct ieee80211vap *vap, u_int8_t *ie);
#if ATH_SUPPORT_WAPI
u_int8_t *ieee80211_setup_wapi_ie(struct ieee80211vap *vap, u_int8_t *ie);
int ieee80211_parse_wapi(struct ieee80211vap *vap, u_int8_t *frm, struct ieee80211_rsnparms *rsn);
#endif
#if ATH_SUPPORT_IBSS_DFS
#define MIN_IBSS_DFS_IE_CONTENT_SIZE            9
#define IBSS_DFS_ZERO_MAP_SIZE                  7
#define DEFAULT_MAX_CSA_MEASREP_ACTION_PER_TBTT 2
#define INIT_IBSS_DFS_OWNER_RECOVERY_TIME_IN_TBTT    10
enum ieee80211_ibss_dfs_state {
    IEEE80211_IBSSDFS_OWNER = 0,
    IEEE80211_IBSSDFS_JOINER = 1,
    IEEE80211_IBSSDFS_WAIT_RECOVERY = 2,
    IEEE80211_IBSSDFS_CHANNEL_SWITCH = 3, 
};

void ieee80211_build_ibss_dfs_ie(struct ieee80211vap *vap);
u_int8_t *ieee80211_add_ibss_dfs(u_int8_t *frm, struct ieee80211vap *vap);
int ieee80211_measurement_report_action(    struct ieee80211vap *vap,     struct ieee80211_measurement_report_ie *pmeasrepie    );
int ieee80211_process_meas_report_ie(struct ieee80211_node *ni, struct ieee80211_action *pia);
#endif /* ATH_SUPPORT_IBSS_DFS */

void  ieee80211_build_countryie(struct ieee80211vap *vap);

int ieee80211_process_csa_ecsa_ie( struct ieee80211_node *ni, struct ieee80211_action *pia);

#if ATH_SUPPORT_CFEND
wbuf_t ieee80211_cfend_alloc(struct ieee80211com *ic);
#endif
/* unalligned little endian access */     
#ifndef LE_READ_2
#define LE_READ_2(p)                            \
    ((u_int16_t)                                \
    ((((const u_int8_t *)(p))[0]      ) |       \
    (((const u_int8_t *)(p))[1] <<  8)))
#endif

#ifndef LE_READ_4
#define LE_READ_4(p)                            \
    ((u_int32_t)                                \
    ((((const u_int8_t *)(p))[0]      ) |       \
    (((const u_int8_t *)(p))[1] <<  8) |        \
    (((const u_int8_t *)(p))[2] << 16) |        \
    (((const u_int8_t *)(p))[3] << 24)))
#endif

#define BE_READ_4(p)                    	\
    ((u_int32_t)                    		\
     ((((const u_int8_t *)(p))[0] << 24) |      \
      (((const u_int8_t *)(p))[1] << 16) |      \
      (((const u_int8_t *)(p))[2] <<  8) |      \
      (((const u_int8_t *)(p))[3]      )))

__inline static int 
iswpaoui(const u_int8_t *frm)
{
    return ((frm[1] > 3) && (LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI)));
}

__inline static int 
isatheros_extcap_oui(const u_int8_t *frm)
{
    return ((frm[1] > 3) && (LE_READ_4(frm+2) == ((ATH_OUI_EXTCAP_TYPE<<24)|ATH_OUI)));
}

INLINE static int 
isatherosoui(const u_int8_t *frm)
{
    return ((frm[1] > 3) && (LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI)));
}

INLINE static int
iswmeoui(const u_int8_t *frm, u_int8_t wme_subtype)
{
    return ((frm[1] > 4) && (LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI)) &&
               (*(frm+6) == wme_subtype));
}

INLINE static int 
iswmeparam(const u_int8_t *frm)
{
    return ((frm[1] > 5) && (LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI)) &&
        (*(frm + 6) == WME_PARAM_OUI_SUBTYPE));
}

INLINE static int 
iswmeinfo(const u_int8_t *frm)
{
    return ((frm[1] > 5) && (LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI)) &&
        (*(frm + 6) == WME_INFO_OUI_SUBTYPE));
}
INLINE static int 
iswmetspec(const u_int8_t *frm)
{
    return ((frm[1] > 5) && (LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI)) &&
        (*(frm + 6) == WME_TSPEC_OUI_SUBTYPE));
}

INLINE static int 
ishtcap(const u_int8_t *frm)
{
    return ((frm[1] > 3) && (BE_READ_4(frm+2) == ((VENDOR_HT_OUI<<8)|VENDOR_HT_CAP_ID)));
}

INLINE static int 
iswpsoui(const u_int8_t *frm)
{
    return frm[1] > 3 && BE_READ_4(frm+2) == WSC_OUI;
}


INLINE static int 
ishtinfo(const u_int8_t *frm)
{
    return ((frm[1] > 3) && (BE_READ_4(frm+2) == ((VENDOR_HT_OUI<<8)|VENDOR_HT_INFO_ID)));
}

INLINE static int 
isssidl(const u_int8_t *frm)
{
    return ((frm[1] > 5) && (LE_READ_4(frm+2) == ((SSIDL_OUI_TYPE<<24)|WPS_OUI)));
}

INLINE static int 
issfaoui(const u_int8_t *frm)
{
    return ((frm[1] > 4) && (LE_READ_4(frm+2) == ((SFA_OUI_TYPE<<24)|SFA_OUI)));
}

INLINE static int 
iswcnoui(const u_int8_t *frm)
{
    return ((frm[1] > 4) && (LE_READ_4(frm+2) == ((WCN_OUI_TYPE<<24)|WCN_OUI)));
}

#undef LE_READ_2
#undef LE_READ_4
#undef BE_READ_4

#endif /* end of _ATH_STA_IEEE80211_PROTO_H */
