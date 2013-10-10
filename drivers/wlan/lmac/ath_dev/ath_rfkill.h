/*
 * Copyright (c) 2009, Atheros Communications Inc.
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

/*
 * Public Interface for RfKill module
 */

#ifndef _DEV_ATH_RFKILL_H
#define _DEV_ATH_RFKILL_H

#include "ath_timer.h"

struct ath_rfkill_info {
    u_int16_t           rf_gpio_select;
    u_int16_t           rf_gpio_polarity;
    HAL_BOOL            rf_hasint;
    HAL_BOOL            rf_phystatechange;
    HAL_BOOL            delay_chk_start;    /* This flag is used by the WAR for
                                             * RfKill Delay; used to skip the
                                             * WAR check for first system boot.
                                             */
    struct ath_timer    rf_timer;
};

int ath_rfkill_attach(struct ath_softc *sc);
void ath_rfkill_detach(struct ath_softc *sc);
void ath_rfkill_start_polling(struct ath_softc *sc);
void ath_rfkill_stop_polling(struct ath_softc *sc);
HAL_BOOL ath_get_rfkill(struct ath_softc *sc);
HAL_BOOL ath_rfkill_gpio_isr(struct ath_softc *sc);
void ath_rfkill_gpio_intr(struct ath_softc *sc);
void ath_rfkill_delay_detect(struct ath_softc *sc);
int ath_rfkill_poll(struct ath_softc *sc);
void ath_enable_RFKillPolling(ath_dev_t dev, int enable);

#define ath_rfkill_hasint(_sc)  ((_sc)->sc_rfkill.rf_hasint)

#endif /* _DEV_ATH_RFKILL_H */
