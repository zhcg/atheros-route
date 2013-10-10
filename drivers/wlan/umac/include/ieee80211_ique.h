/*
 *  Copyright (c) 2009 Atheros Communications Inc.
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
 *
 *  Header file for IQUE feature.
 */

#ifndef _IEEE80211_IQUE_H_
#define _IEEE80211_IQUE_H_

#include "osdep.h"
#include "wbuf.h"
#include "ieee80211.h"

struct ieee80211_node;
struct ieee80211vap;

/*
 * mcast enhancement ops
 */
struct ieee80211_ique_ops {
    /*
     * functions for mcast enhancement
     */
    void    (*me_detach)(struct ieee80211vap *);
    void    (*me_inspect)(struct ieee80211vap *, struct ieee80211_node *, wbuf_t);
    int     (*me_convert)(struct ieee80211vap *, wbuf_t);
    void    (*me_dump)(struct ieee80211vap *);
    void    (*me_clean)(struct ieee80211_node *);
    void    (*me_showdeny)(struct ieee80211vap *);
    void    (*me_adddeny)(struct ieee80211vap *, int *addr);
    void    (*me_cleardeny)(struct ieee80211vap *);    
    /*
     * functions for headline block removal (HBR)
     */
    void    (*hbr_detach)(struct ieee80211vap *);
    void    (*hbr_nodejoin)(struct ieee80211vap *, struct ieee80211_node *);
    void    (*hbr_nodeleave)(struct ieee80211vap *, struct ieee80211_node *);
    int     (*hbr_dropblocked)(struct ieee80211vap *, struct ieee80211_node *, wbuf_t);
    void    (*hbr_set_probing)(struct ieee80211vap *, struct ieee80211_node *, wbuf_t, u_int8_t);
    void    (*hbr_sendevent)(struct ieee80211vap *, u_int8_t *, int);
    void    (*hbr_dump)(struct ieee80211vap *);
    void    (*hbr_settimer)(struct ieee80211vap *, u_int32_t);
};

#if ATH_SUPPORT_IQUE

#ifndef MAX_SNOOP_ENTRIES
#define MAX_SNOOP_ENTRIES	32	/* max number*/
#endif
#ifndef DEF_SNOOP_ENTRIES
#define DEF_SNOOP_ENTRIES	8	/* default list length */
#endif

#define DEF_ME_TIMER	30000	/* timer interval as 30 secs */
#define DEF_ME_TIMEOUT	120000	/* 2 minutes for timeout  */

#define GRPADDR_FILTEROUT_N 8

/*
 * Data structures for mcast enhancement
 */

typedef rwlock_t ieee80211_snoop_lock_t;

/* TODO: Demo uses single combo list, not optimized */
/* list entry */
struct MC_GROUP_LIST_ENTRY {
    u_int8_t							mgl_group_addr[IEEE80211_ADDR_LEN];
    u_int8_t							mgl_group_member[IEEE80211_ADDR_LEN];
    u_int32_t							mgl_timestamp;
    u_int32_t                                                   vlan_id;
    unsigned int                                                mgl_xmited;
    struct ieee80211_node                                       *mgl_ni;
    TAILQ_ENTRY(MC_GROUP_LIST_ENTRY)                            mgl_list;
};

/* global (for demo only) struct to manage snoop */
struct MC_SNOOP_LIST {
	u_int16_t							msl_group_list_count;
	u_int16_t							msl_misc;
	u_int16_t							msl_max_length;
    ieee80211_snoop_lock_t				msl_lock;	/* lock snoop table */
    u_int8_t                            msl_deny_group[GRPADDR_FILTEROUT_N][4];
    u_int8_t                            msl_deny_mask[GRPADDR_FILTEROUT_N][4];
    u_int8_t                            msl_deny_count;
    TAILQ_HEAD(MSL_HEAD_TYPE, MC_GROUP_LIST_ENTRY)	msl_node;	/* head of list of all snoop entries */
};

struct ieee80211_ique_me {
	struct ieee80211vap *me_iv;	/* Backward pointer to vap instance */
	struct MC_SNOOP_LIST ieee80211_me_snooplist;
    u_int32_t       me_timer;		/* Timer to maintain the snoop list */
    u_int32_t       me_timeout;		/* Timeout to delete entry from the snoop list if no traffic */
    int             mc_snoop_enable;
    int             mc_discard_mcast;   /*discard the mcast frames if empty table entry*/
    int             me_debug;	
	os_timer_t      snooplist_timer;
	spinlock_t	    ieee80211_melock;
};

/*
 * For HBR (headline block removal)
 */

typedef enum {
    IEEE80211_HBR_STATE_ACTIVE, 
    IEEE80211_HBR_STATE_BLOCKING, 
    IEEE80211_HBR_STATE_PROBING, 
} ieee80211_hbr_state; 

typedef enum {
    IEEE80211_HBR_EVENT_BACK,
    IEEE80211_HBR_EVENT_FORWARD,
    IEEE80211_HBR_EVENT_STALE,
}ieee80211_hbr_event;


struct ieee80211_hbr_list;

int ieee80211_me_attach(struct ieee80211vap * vap);
int ieee80211_hbr_attach(struct ieee80211vap * vap);

#define ieee80211_ique_attach(_vap) do {ieee80211_me_attach(_vap); ieee80211_hbr_attach(_vap);} while(0)

#else

#define ieee80211_ique_attach(_vap) do { OS_MEMZERO(&((_vap)->iv_ique_ops), sizeof(struct ieee80211_ique_ops)); } while(0) 

#endif /*ATH_SUPPORT_IQUE*/

#endif /* _IEEE80211_IQUE_H_ */
