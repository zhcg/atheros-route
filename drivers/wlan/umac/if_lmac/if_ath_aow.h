/*
 * Copyright (c) 2010 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _ATH_AOW_H_
#define _ATH_AOW_H_

#if  ATH_SUPPORT_AOW

extern void ath_net80211_aow_send2all_nodes(void *reqvap, void *data, int len, u_int32_t seqno, u_int64_t tsf);
extern void ath_aow_attach(struct ieee80211com* ic, struct ath_softc_net80211 *scn);
extern int  ath_net80211_aow_consume_audio_data(ieee80211_handle_t ieee, u_int64_t tsf);
extern void ath_list_audio_channel(struct ieee80211com *ic);
extern void ath_aow_l2pe_record(struct ieee80211com *ic, bool is_success);
extern u_int32_t ath_aow_chksum_crc32 (unsigned char *block,
                                       unsigned int length);
extern u_int32_t ath_aow_cumultv_chksum_crc32(u_int32_t crc_prev,
                                              unsigned char *block,
                                              unsigned int length);
#else   /* ATH_SUPPORT_AOW */

#define ath_aow_attach(a,b) do{}while(0)
#define ath_aow_l2pe_record(a,b) do{}while(0)
#define ath_aow_chksum_crc32(a,b) do{}while(0)
#define ath_aow_cumultv_chksum_crc32(a,b,c) do{}while(0)

#endif  /* ATH_SUPPORT_AOW */

#endif  /* _ATH_AOW_H_ */
