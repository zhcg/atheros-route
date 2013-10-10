/*
 * Copyright (c) 2010, Atheros Communications Inc.
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

#ifndef IEEE80211_WDS_H
#define IEEE80211_WDS_H

#include <if_upperproto.h>
#include <ieee80211_var.h>
#include <ieee80211_scan.h>
#include <ieee80211_node.h>


#if UMAC_SUPPORT_WDS

void
ieee80211_wds_attach(struct ieee80211_node_table *nt);
void
ieee80211_wds_detach(struct ieee80211_node_table *nt);

/* Add wds address to the node table */
int
ieee80211_add_wds_addr(struct ieee80211_node_table *nt,
		       struct ieee80211_node *ni, const u_int8_t *macaddr,
		       u_int32_t flags);

/* remove wds address from the wds hash table */
void
ieee80211_remove_wds_addr(struct ieee80211_node_table *nt,
			  const u_int8_t *macaddr,u_int32_t flags);
/* Remove node references from wds table */
void
ieee80211_del_wds_node(struct ieee80211_node_table *nt,
		       struct ieee80211_node *ni);

/* Remove all the wds entries associated with the AP when the AP to
 * which STA is associated goes down
 */
int ieee80211_node_removeall_wds (struct ieee80211_node_table *nt,struct ieee80211_node *ni);

struct ieee80211_node *
ieee80211_find_wds_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr);
void
ieee80211_set_wds_node_time(struct ieee80211_node_table *nt, const u_int8_t *macaddr);
u_int32_t
ieee80211_find_wds_node_age(struct ieee80211_node_table *nt, const u_int8_t *macaddr);
int ieee80211_node_wdswar_isaggrdeny(struct ieee80211_node *ni);
int ieee80211_node_wdswar_issenddelba(struct ieee80211_node *ni);
void wds_clear_wds_table(struct ieee80211_node * ni, struct ieee80211_node_table *nt, wbuf_t wbuf );
void wds_update_rootwds_table(struct ieee80211_node * ni, struct ieee80211_node_table *nt, wbuf_t wbuf );
int wds_sta_chkmcecho(struct ieee80211_node_table * nt, u_int8_t * sender );
extern struct ieee80211_node * 
    _ieee80211_find_node( struct ieee80211_node_table *nt,
            const u_int8_t *macaddr);

extern void _ieee80211_free_node(struct ieee80211_node *ni);

/* Function to check if the packet has to be sent as 4 addr packet
 * Takes as input ni, wbuf and returns 1 if 4 address is required
 * returns 0 if 4 address is not required
 */

static INLINE int
wds_is4addr(struct ieee80211vap * vap, struct ether_header eh , const u_int8_t * macaddr)
{
    if (IEEE80211_VAP_IS_WDS_ENABLED(vap) && 
	!(IEEE80211_ADDR_EQ(eh.ether_dhost, macaddr)))
	{
	    return 1;
	}
    else
	{
            return 0;
	}
}

#else  // UMAC_SUPPORT_WDS

static INLINE void
ieee80211_wds_attach(struct ieee80211_node_table *nt)
{
    return;
}
static INLINE void
ieee80211_wds_detach(struct ieee80211_node_table *nt)
{
    return;
}

/* Add wds address to the node table */
static INLINE int
ieee80211_add_wds_addr(struct ieee80211_node_table *nt,
		       struct ieee80211_node *ni, const u_int8_t *macaddr,
                       u_int32_t flags)
{
    return 0;
}

/* remove wds address from the wds hash table */
static INLINE void
ieee80211_remove_wds_addr(struct ieee80211_node_table *nt,
			  const u_int8_t *macaddr,u_int32_t flags)
{
    return;
}
/* Remove node references from wds table */
static INLINE void
ieee80211_del_wds_node(struct ieee80211_node_table *nt,
		       struct ieee80211_node *ni)
{
    return;
}

/* Remove all the wds entries associated with the AP when the AP to
 * which STA is associated goes down
 */
static INLINE int ieee80211_node_removeall_wds (struct ieee80211_node_table *nt,struct ieee80211_node *ni)
{
    return 0;
}

static INLINE struct ieee80211_node *
ieee80211_find_wds_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
    return NULL;
}

static INLINE void
ieee80211_set_wds_node_time(struct ieee80211_node_table *nt, const u_int8_t *macaddr);
{
    return;
}

static INLINE u_int32_t
ieee80211_find_wds_node_age(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
    return 0;
}
static INLINE int ieee80211_node_wdswar_isaggrdeny(struct ieee80211_node *ni)
{
    return 0;
}
static INLINE int ieee80211_node_wdswar_issenddelba(struct ieee80211_node *ni)
{
    return 0;
}

static INLINE void wds_clear_wds_table(struct ieee80211_node * ni, struct ieee80211_node_table *nt, wbuf_t wbuf )
{
    return;
}

static INLINE void wds_update_rootwds_table(struct ieee80211_node * ni, struct ieee80211_node_table *nt, wbuf_t wbuf )
{
    return;
}
static INLINE int 
wds_sta_chkmcecho(struct ieee80211_node_table * nt, u_int8_t * sender )
{
    return 0;
}

/* Function to check if the packet has to be sent as 4 addr packet
 * Takes as input ni, wbuf and returns 1 if 4 address is required
 * returns 0 if 4 address is not required
 */
static INLINE int
wds_is4addr(struct ieee80211vap * vap, struct ether_header eh , const u_int8_t * macaddr)
{
    return 0;
}


#endif //UMAC_SUPPORT_WDS

#if UMAC_SUPPORT_NAWDS
void ieee80211_nawds_attach(struct ieee80211vap *vap);
int ieee80211_nawds_send_wbuf(struct ieee80211vap *vap, wbuf_t wbuf);
int ieee80211_nawds_disable_beacon(struct ieee80211vap *vap);
int ieee80211_nawds_enable_learning(struct ieee80211vap *vap);
void ieee80211_nawds_learn(struct ieee80211vap *vap, u_int8_t *mac);
#else
static INLINE void ieee80211_nawds_attach(struct ieee80211vap *vap) 
{
    /* do nothing */
};
static INLINE int ieee80211_nawds_send_wbuf(struct ieee80211vap *vap, wbuf_t wbuf)
{
    /* do nothing */
    return 0;
}

static INLINE int ieee80211_nawds_disable_beacon(struct ieee80211vap *vap)
{
    /* do nothing */
    return 0;
}

static INLINE int ieee80211_nawds_enable_learning(struct ieee80211vap *vap)
{
    /* do nothing */
    return 0;
}

static INLINE void ieee80211_nawds_learn(struct ieee80211vap *vap, u_int8_t *mac)
{
    /* do nothing */
}

#endif

#endif //IEEE80211_WDS_H
