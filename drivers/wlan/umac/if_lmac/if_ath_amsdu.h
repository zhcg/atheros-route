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
#ifndef _ATH_AMSDU_H_
#define _ATH_AMSDU_H_

/*
 * AMSDU Frame
 */
#define	ATH_AMSDU_TXQ_LOCK_INIT(_scn)    spin_lock_init(&(_scn)->amsdu_txq_lock)
#define	ATH_AMSDU_TXQ_LOCK_DESTROY(_scn)
#define	ATH_AMSDU_TXQ_LOCK(_scn)     spin_lock(&(_scn)->amsdu_txq_lock)
#define	ATH_AMSDU_TXQ_UNLOCK(_scn)   spin_unlock(&(_scn)->amsdu_txq_lock)

/*
 * External Definitions
 */
struct ath_amsdu_tx {
    wbuf_t                  amsdu_tx_buf; /* amsdu staging area */
    TAILQ_ENTRY(ath_amsdu_tx) amsdu_qelem; /* round-robin amsdu tx entry */
    u_int                   sched;
};

struct ath_amsdu {
    struct ath_amsdu_tx     amsdutx[WME_NUM_TID];
};

int  ath_amsdu_attach(struct ath_softc_net80211 *scn);
void ath_amsdu_detach(struct ath_softc_net80211 *scn);
int  ath_amsdu_node_attach(struct ath_softc_net80211 *scn, struct ath_node_net80211 *anode);
void ath_amsdu_node_detach(struct ath_softc_net80211 *scn, struct ath_node_net80211 *anode);
wbuf_t ath_amsdu_send(wbuf_t wbuf);
void ath_amsdu_complete_wbuf(wbuf_t wbuf);
void ath_amsdu_tx_drain(struct ath_softc_net80211 *scn);

#endif /* _ATH_AMSDU_H_ */
