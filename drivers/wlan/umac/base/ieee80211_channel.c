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

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_api.h>

const char *ieee80211_phymode_name[] = {
    "auto",             /* IEEE80211_MODE_AUTO */
    "11a",              /* IEEE80211_MODE_11A */
    "11b",              /* IEEE80211_MODE_11B */
    "11g",              /* IEEE80211_MODE_11G */
    "FH",               /* IEEE80211_MODE_FH */
    "turboA",           /* IEEE80211_MODE_TURBO_A */
    "turboG",           /* IEEE80211_MODE_TURBO_G */
    "11naht20",         /* IEEE80211_MODE_11NA_HT20 */
    "11nght20",         /* IEEE80211_MODE_11NG_HT20 */
    "11naht40plus",     /* IEEE80211_MODE_11NA_HT40PLUS */
    "11naht40minus",    /* IEEE80211_MODE_11NA_HT40MINUS */
    "11nght40plus",     /* IEEE80211_MODE_11NG_HT40PLUS */
    "11nght40minus",    /* IEEE80211_MODE_11NG_HT40MINUS */
    "11nght40",         /* IEEE80211_MODE_11NG_HT40 */
    "11naht40",         /* IEEE80211_MODE_11NA_HT40 */
};

/*
 * Return the phy mode for with the specified channel.
 */
enum ieee80211_phymode
ieee80211_chan2mode(const struct ieee80211_channel *chan)
{
    if (IEEE80211_IS_CHAN_108G(chan))
        return IEEE80211_MODE_TURBO_G;
    else if (IEEE80211_IS_CHAN_TURBO(chan))
        return IEEE80211_MODE_TURBO_A;
    else if (IEEE80211_IS_CHAN_A(chan))
        return IEEE80211_MODE_11A;
    else if (IEEE80211_IS_CHAN_ANYG(chan))
        return IEEE80211_MODE_11G;
    else if (IEEE80211_IS_CHAN_B(chan))
        return IEEE80211_MODE_11B;
    else if (IEEE80211_IS_CHAN_FHSS(chan))
        return IEEE80211_MODE_FH;
    else if (IEEE80211_IS_CHAN_11NA_HT20(chan))
        return IEEE80211_MODE_11NA_HT20;
    else if (IEEE80211_IS_CHAN_11NG_HT20(chan))
        return IEEE80211_MODE_11NG_HT20;
    else if (IEEE80211_IS_CHAN_11NA_HT40PLUS(chan))
        return IEEE80211_MODE_11NA_HT40PLUS;
    else if (IEEE80211_IS_CHAN_11NA_HT40MINUS(chan))
        return IEEE80211_MODE_11NA_HT40MINUS;
    else if (IEEE80211_IS_CHAN_11NG_HT40PLUS(chan))
        return IEEE80211_MODE_11NG_HT40PLUS;
    else if (IEEE80211_IS_CHAN_11NG_HT40MINUS(chan))
        return IEEE80211_MODE_11NG_HT40MINUS;

    /* NB: should not get here */
    printk("%s: cannot map channel to mode; freq %u flags 0x%x\n",
           __func__, chan->ic_freq, chan->ic_flags);
    return IEEE80211_MODE_11B;
}

/*
 * Check for Channel and Mode consistency. If channel and mode mismatches, return error.
 */
#define IEEE80211_MODE_TURBO_STATIC_A   IEEE80211_MODE_MAX
static int
ieee80211_check_chan_mode_consistency(struct ieee80211com *ic,int mode,struct ieee80211_channel *c)
{
    if (c == IEEE80211_CHAN_ANYC) return 0;
    switch (mode)
    {
    case IEEE80211_MODE_11B:
        if(IEEE80211_IS_CHAN_B(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11G:
        if(IEEE80211_IS_CHAN_ANYG(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11A:
        if(IEEE80211_IS_CHAN_A(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_TURBO_STATIC_A:
        if(IEEE80211_IS_CHAN_A(c) && IEEE80211_IS_CHAN_STURBO(c) )
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_AUTO:
        return 0;
        break;

    case IEEE80211_MODE_11NG_HT20:
        if(IEEE80211_IS_CHAN_11NG_HT20(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11NG_HT40PLUS:
        if(IEEE80211_IS_CHAN_11NG_HT40PLUS(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11NG_HT40MINUS:
        if(IEEE80211_IS_CHAN_11NG_HT40MINUS(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11NG_HT40:
        if(IEEE80211_IS_CHAN_11NG_HT40MINUS(c) || IEEE80211_IS_CHAN_11NG_HT40PLUS(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11NA_HT20:
        if(IEEE80211_IS_CHAN_11NA_HT20(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11NA_HT40PLUS:
        if(IEEE80211_IS_CHAN_11NA_HT40PLUS(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11NA_HT40MINUS:
        if(IEEE80211_IS_CHAN_11NA_HT40MINUS(c))
            return 0;
        else
            return -EINVAL;
        break;

    case IEEE80211_MODE_11NA_HT40:
        if(IEEE80211_IS_CHAN_11NA_HT40MINUS(c) || IEEE80211_IS_CHAN_11NA_HT40PLUS(c))
            return 0;
        else
            return -EINVAL;
        break;
    }
    return -EINVAL;
}
#undef  IEEE80211_MODE_TURBO_STATIC_A

/*
 * Convert MHz frequency to IEEE channel number.
 */
u_int
ieee80211_mhz2ieee(struct ieee80211com *ic, u_int freq, u_int flags)
{
    /* XXX: The only reason to call low-level driver here is to
     * let HAL take care of public safety channel configurations. */
    return ic->ic_mhz2ieee(ic, freq, flags);
}

/*
 * Convert IEEE channel number to MHz frequency.
 */
u_int
ieee80211_ieee2mhz(u_int chan, u_int flags)
{
    if (flags & IEEE80211_CHAN_2GHZ) {  /* 2GHz band */
        if (chan == 14)
            return 2484;
        if (chan < 14)
            return 2407 + chan*5;
        else
            return 2512 + ((chan-15)*20);
    } else if (flags & IEEE80211_CHAN_5GHZ) {/* 5Ghz band */
        return 5000 + (chan*5);
    } else {                /* either, guess */
        if (chan == 14)
            return 2484;
        if (chan < 14)          /* 0-13 */
            return 2407 + chan*5;
        if (chan < 27)          /* 15-26 */
            return 2512 + ((chan-15)*20);
        return 5000 + (chan*5);
    }
}

u_int32_t
wlan_mhz2ieee(wlan_dev_t devhandle, u_int32_t freq, u_int32_t flags)
{
    struct ieee80211com *ic = devhandle;

    return ieee80211_mhz2ieee(ic, freq, flags);
}

u_int32_t
wlan_ieee2mhz(wlan_dev_t devhandle, int ieeechan)
{
    return ieee80211_ieee2mhz(ieeechan, 0);
}

/*
 * Locate a channel given a frequency+flags.  We cache
 * the previous lookup to optimize swithing between two
 * channels--as happens with dynamic turbo.
 * This verifies that found channels have not been excluded because of 11d.
 */
struct ieee80211_channel *
ieee80211_find_channel(struct ieee80211com *ic, int freq, int flags)
{
    struct ieee80211_channel *c;
    int i;

    flags &= IEEE80211_CHAN_ALLTURBO;
    c = ic->ic_prevchan;
    if ((c != NULL) && 
        (! IEEE80211_IS_CHAN_11D_EXCLUDED(c)) &&
        (c->ic_freq == freq) &&
        ((c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)) {
        return c;
    }

    /* brute force search */
    for (i = 0; i < ic->ic_nchans; i++) {
        c = &ic->ic_channels[i];

        if ((! IEEE80211_IS_CHAN_11D_EXCLUDED(c)) &&
            (c->ic_freq == freq) &&
            ((c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)) {
            return c;
        }
    }

    return NULL;
}

struct ieee80211_channel *
ieee80211_doth_findchan(struct ieee80211vap *vap, u_int8_t chan)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_channel *c;
    int flags, freq;

    /* NB: try first to preserve turbo */
    flags = vap->iv_bsschan->ic_flags & IEEE80211_CHAN_ALL;
    freq = ieee80211_ieee2mhz(chan, 0);
    c = ieee80211_find_channel(ic, freq, flags);
    if (c == NULL)
        c = ieee80211_find_channel(ic, freq, 0);
    return c;
}

/*
 * Locate the channel given channel number and mode
 */
struct ieee80211_channel *
ieee80211_find_dot11_channel(struct ieee80211com *ic, int ieee, enum ieee80211_phymode mode)
{
    static const u_int chanflags[] = {
        0,                              /* IEEE80211_MODE_AUTO */
        IEEE80211_CHAN_A,               /* IEEE80211_MODE_11A */
        IEEE80211_CHAN_B,               /* IEEE80211_MODE_11B */
        IEEE80211_CHAN_PUREG,           /* IEEE80211_MODE_11G */
        IEEE80211_CHAN_FHSS,            /* IEEE80211_MODE_FH */
        IEEE80211_CHAN_108A,            /* IEEE80211_MODE_TURBO_A */
        IEEE80211_CHAN_108G,            /* IEEE80211_MODE_TURBO_G */
        IEEE80211_CHAN_11NA_HT20,       /* IEEE80211_MODE_11NA_HT20 */
        IEEE80211_CHAN_11NG_HT20,       /* IEEE80211_MODE_11NG_HT20 */
        IEEE80211_CHAN_11NA_HT40PLUS,   /* IEEE80211_MODE_11NA_HT40PLUS */
        IEEE80211_CHAN_11NA_HT40MINUS,  /* IEEE80211_MODE_11NA_HT40MINUS */
        IEEE80211_CHAN_11NG_HT40PLUS,   /* IEEE80211_MODE_11NG_HT40PLUS */
        IEEE80211_CHAN_11NG_HT40MINUS,  /* IEEE80211_MODE_11NG_HT40MINUS */
        0,                              /* IEEE80211_MODE_11NG_HT40 */
        0,                              /* IEEE80211_MODE_11NA_HT40 */
        IEEE80211_CHAN_ST,              /* IEEE80211_MODE_TURBO_STATIC_A */
    };
    u_int modeflags;
    int i;

    modeflags = mode & (IEEE80211_CHAN_HALF | IEEE80211_CHAN_QUARTER);
    mode &= ~modeflags;
    modeflags |= chanflags[mode];

    for (i = 0; i < ic->ic_nchans; i++) {
        struct ieee80211_channel *c = &ic->ic_channels[i];
        struct ieee80211_channel *nc;

        if (ieee && (c->ic_ieee != ieee))
            continue;

        if (mode == IEEE80211_MODE_AUTO) {
            if (IEEE80211_IS_CHAN_TURBO(c)) {
                /* ignore turbo channels for autoselect */
                continue;
            }

            if (IEEE80211_IS_CHAN_2GHZ(c)) {
                /*
                 * Precedence is 11NG channels first, then 11G, then 11B.
                 * HT40 channels are ignored when IEEE80211_MODE_AUTO is used.
                 */
                if ((ic->ic_modecaps & (1<<IEEE80211_MODE_11NG_HT20)) &&
                    ((ic->ic_opmode != IEEE80211_M_IBSS) || ieee80211_ic_ht20Adhoc_is_set(ic)) &&
                    ((nc = ieee80211_find_channel(ic, c->ic_freq, IEEE80211_CHAN_11NG_HT20)) != NULL)) {
                    return nc;
                } else if ((ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) &&
                           ((nc = ieee80211_find_channel(ic, c->ic_freq, IEEE80211_CHAN_PUREG)) != NULL)) {
                    return nc;
                } else if ((ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) &&
                           ((nc = ieee80211_find_channel(ic, c->ic_freq, IEEE80211_CHAN_G)) != NULL)) {
                    return nc;
                } else if ((ic->ic_modecaps & (1<<IEEE80211_MODE_11B)) &&
                           ((nc = ieee80211_find_channel(ic, c->ic_freq, IEEE80211_CHAN_B)) != NULL)) {
                    return nc;
                }
            } else {
                /* Precedence is 11NA channels first, then 11A */
                if ((ic->ic_modecaps & (1<<IEEE80211_MODE_11NA_HT20)) &&
                    ((ic->ic_opmode != IEEE80211_M_IBSS) || ieee80211_ic_ht20Adhoc_is_set(ic)) &&
                    ((nc = ieee80211_find_channel(ic, c->ic_freq, IEEE80211_CHAN_11NA_HT20)) != NULL)) {
                    return nc;
                } else if ((ic->ic_modecaps & (1<<IEEE80211_MODE_11A)) &&
                           ((nc = ieee80211_find_channel(ic, c->ic_freq, IEEE80211_CHAN_A)) != NULL)) {
                    return nc;
                } 
            }
        } else if (mode == IEEE80211_MODE_11NG_HT40) {
            if (IEEE80211_IS_CHAN_2GHZ(c)) {
                 /* Precedence is 11NG PLUS channels first */

                if ((c->ic_flags & IEEE80211_CHAN_11NG_HT40PLUS) == IEEE80211_CHAN_11NG_HT40PLUS)
                    return c;

                if ((c->ic_flags & IEEE80211_CHAN_11NG_HT40MINUS) == IEEE80211_CHAN_11NG_HT40MINUS)
                    return c;
            }
        } else if (mode == IEEE80211_MODE_11NA_HT40) {
            if (IEEE80211_IS_CHAN_5GHZ(c)) {
                 /* Precedence is 11NA PLUS channels first */

                if ((c->ic_flags & IEEE80211_CHAN_11NA_HT40PLUS) == IEEE80211_CHAN_11NA_HT40PLUS)
                    return c;

                if ((c->ic_flags & IEEE80211_CHAN_11NA_HT40MINUS) == IEEE80211_CHAN_11NA_HT40MINUS)
                    return c;
            }
        } else {
            if ((c->ic_flags & modeflags) == modeflags)
                return c;
            }
        }
    return NULL;
}

/*
 * Check if two channels are in the same frequency band.
 */
bool
ieee80211_is_same_frequency_band(const struct ieee80211_channel *chan1, const struct ieee80211_channel *chan2)
{
    if (IEEE80211_IS_CHAN_2GHZ(chan1)) {
        /*
         * Channel1 is 2GHz, return TRUE only if channel2 is also 2GHz
         */
        return IEEE80211_IS_CHAN_2GHZ(chan2);
    }
    else {
        /*
         * Channel1 is 5GHz, return TRUE only if channel2 is also 5GHz
         */
        return IEEE80211_IS_CHAN_5GHZ(chan2);
    }
}

/*
 * Update channel list and associated PHY mode bitmask
 */
void
ieee80211_update_channellist(struct ieee80211com *ic, int exclude_11d)
{
    struct ieee80211_channel *c;
    int i;
    u_int16_t modcapmask;

    /*
     * Fill in 802.11 available channel set, mark
     * all available channels as active, and pick
     * a default channel if not already specified.
     */
    KASSERT(0 < ic->ic_nchans && ic->ic_nchans <= IEEE80211_CHAN_MAX,
            ("invalid number of channels specified: %u", ic->ic_nchans));
    OS_MEMZERO(ic->ic_chan_avail, sizeof(ic->ic_chan_avail));

    modcapmask = (1 << IEEE80211_MODE_MAX) -1;
    ic->ic_modecaps &= (~modcapmask);
    ic->ic_modecaps |= 1<<IEEE80211_MODE_AUTO;

    for (i = 0; i < ic->ic_nchans; i++) {
        c = &ic->ic_channels[i];
        KASSERT(c->ic_flags != 0, ("channel with no flags"));
        KASSERT(c->ic_ieee < IEEE80211_CHAN_MAX,
                ("channel with bogus ieee number %u", c->ic_ieee));

        if (exclude_11d && IEEE80211_IS_CHAN_11D_EXCLUDED(c))
            continue;
            
        setbit(ic->ic_chan_avail, c->ic_ieee);

        /*
         * Identify mode capabilities.
         */
        if (IEEE80211_IS_CHAN_A(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11A;
        if (IEEE80211_IS_CHAN_B(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11B;
        if (IEEE80211_IS_CHAN_PUREG(c) || IEEE80211_IS_CHAN_G(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11G;
        if (IEEE80211_IS_CHAN_FHSS(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_FH;
        if (IEEE80211_IS_CHAN_108A(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_A;
        if (IEEE80211_IS_CHAN_108G(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_G;
        if (IEEE80211_IS_CHAN_11NA_HT20(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA_HT20;
        if (IEEE80211_IS_CHAN_11NG_HT20(c))
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG_HT20;
        if (IEEE80211_IS_CHAN_11NA_HT40PLUS(c)) {
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA_HT40PLUS;
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA_HT40;
        }
        if (IEEE80211_IS_CHAN_11NA_HT40MINUS(c)) {
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA_HT40MINUS;
            ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA_HT40;
        }
        
        /*
         * HT40 in 2GHz allowed only if user enabled it.
         */
        if (ic->ic_reg_parm.enable2GHzHt40Cap) {
            if (IEEE80211_IS_CHAN_11NG_HT40PLUS(c)) {
                ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG_HT40PLUS;
                ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG_HT40;
            }
            if (IEEE80211_IS_CHAN_11NG_HT40MINUS(c)) {
                ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG_HT40MINUS;
                ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG_HT40;
            }
        }
    }

    /* initialize candidate channels to all available */
    OS_MEMCPY(ic->ic_chan_active, ic->ic_chan_avail,
              sizeof(ic->ic_chan_avail));
    
    if (exclude_11d)
        ieee80211_scan_update_channel_list(ic->ic_scanner);
}

#if UMAC_SUPPORT_IBSS
struct ieee80211_channel *
ieee80211_autoselect_adhoc_channel(struct ieee80211com *ic)
{
    struct ieee80211_channel *c;
    int i;

    /*
     * If 11n adhoc is enabled, We do 11NG_HT20, then G, then B modes.
     */
    if (ieee80211_ic_ht20Adhoc_is_set(ic)) {
        c = ieee80211_find_dot11_channel(ic, IEEE80211_CHAN_ADHOC_DEFAULT1, IEEE80211_MODE_11NG_HT20);
        if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
            return c;
        c = ieee80211_find_dot11_channel(ic, IEEE80211_CHAN_ADHOC_DEFAULT2, IEEE80211_MODE_11NG_HT20);
        if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
            return c;

        for (i = 0; i < ic->ic_nchans; i++) {
            c = ieee80211_find_dot11_channel(ic, ic->ic_channels[i].ic_ieee, IEEE80211_MODE_11NG_HT20);
            if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
                return c;
        }
    }

    /* 
     * Give preference to 802.11g channel 10 because it is the safest channel
     * to be usded in ad hoc, i.e. least regulartory domain consideration.
     */
    c = ieee80211_find_dot11_channel(ic, IEEE80211_CHAN_ADHOC_DEFAULT1, IEEE80211_MODE_11G);
    if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
        return c;

    /* Then 802.11g channel 11  */
    c = ieee80211_find_dot11_channel(ic, IEEE80211_CHAN_ADHOC_DEFAULT2, IEEE80211_MODE_11G);
    if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
        return c;

    /* Then any 802.11g channel */
    for (i = 0; i < ic->ic_nchans; i++) {
        c = &ic->ic_channels[i];
        if (IEEE80211_IS_CHAN_ANYG(c) && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
            return c;
    }

    /* Then 802.11b channel 10  */
    c = ieee80211_find_dot11_channel(ic, IEEE80211_CHAN_ADHOC_DEFAULT1, IEEE80211_MODE_11B);
    if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
        return c;

    /* Then 802.11b channel 11  */
    c = ieee80211_find_dot11_channel(ic, IEEE80211_CHAN_ADHOC_DEFAULT2, IEEE80211_MODE_11B);
    if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
        return c;

    /* Then any 802.11b channel */
    for (i = 0; i < ic->ic_nchans; i++) {
        c = &ic->ic_channels[i];
        if (IEEE80211_IS_CHAN_B(c) && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
            return c;
    }

    /*
     * If 11n adhoc is enabled, We do 11NA_HT20, then A mode.
     */
    if (ieee80211_ic_ht20Adhoc_is_set(ic)) {
        for (i = 0; i < ic->ic_nchans; i++) {
            c = ieee80211_find_dot11_channel(ic, ic->ic_channels[i].ic_ieee, IEEE80211_MODE_11NA_HT20);
            if (c && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
                return c;
        }
    }

    /* Then any 802.11a channel */
    for (i = 0; i < ic->ic_nchans; i++) {
        c = &ic->ic_channels[i];
        if (IEEE80211_IS_CHAN_A(c) && !IEEE80211_IS_CHAN_DISALLOW_ADHOC(c))
            return c;
    }
    
    return NULL;
}
#endif

/*
 * Set current channel
 */
int
ieee80211_set_channel(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
    int error;
    ic->ic_prevchan = ic->ic_curchan;
    ic->ic_curchan = chan;
    error = ic->ic_set_channel(ic);
    ic->ic_curmode = ieee80211_chan2mode(ic->ic_curchan);
    return error;
}

/*
 * Set the current phy mode.
 */
int ieee80211_setmode(struct ieee80211com *ic,
                      enum ieee80211_phymode mode,
                      enum ieee80211_opmode opmode)
{
    ieee80211_reset_erp(ic, mode, opmode); /* reset ERP state */
    ic->ic_curmode = mode;      /* NB: must do post reset_erp */
    return 0;
}

const char *ieee80211_phymode_to_name( enum ieee80211_phymode mode)
{
    static const char unknown_mode[]="UNKNWON";
    if (mode >= sizeof(ieee80211_phymode_name)/sizeof(char *)) {
        return unknown_mode;
    }
    return (ieee80211_phymode_name[mode]);
}

int
wlan_get_supported_phymodes(wlan_dev_t devhandle,
                            enum ieee80211_phymode *modes,
                            u_int16_t *nmodes,
                            u_int16_t len)
{
    struct ieee80211com *ic = devhandle;
    int n = 0;
#define ADD_PHY_MODE(_m) do {                       \
    if (IEEE80211_SUPPORT_PHY_MODE(ic, (_m))) {     \
        if (len < (n+1))                            \
            goto bad;                               \
        modes[n++] = (_m);                          \
    }                                               \
} while (0)

    /*
     * NB: we fill in the modes array in certain order to be
     * compatible with Win 7/Vista SP1 driver.
     */
    ADD_PHY_MODE(IEEE80211_MODE_11B);
    ADD_PHY_MODE(IEEE80211_MODE_11A);
    ADD_PHY_MODE(IEEE80211_MODE_11G);
    ADD_PHY_MODE(IEEE80211_MODE_11NG_HT40PLUS);
    ADD_PHY_MODE(IEEE80211_MODE_11NG_HT40MINUS);
    ADD_PHY_MODE(IEEE80211_MODE_11NG_HT20);
    ADD_PHY_MODE(IEEE80211_MODE_11NA_HT40PLUS);
    ADD_PHY_MODE(IEEE80211_MODE_11NA_HT40MINUS);
    ADD_PHY_MODE(IEEE80211_MODE_11NA_HT20);

    *nmodes = n;
    return 0;

bad:
    *nmodes = IEEE80211_MODE_MAX;
    return -EOVERFLOW;
#undef ADD_PHY_MODE
}

int
wlan_set_desired_phylist(wlan_if_t vaphandle, enum ieee80211_phymode *phylist, u_int16_t nphy)
{
    struct ieee80211vap *vap = vaphandle;
    int i;

    if (nphy > IEEE80211_MODE_MAX || nphy < 1) {
        return -EINVAL;
    }

    vap->iv_des_modecaps = 0;
    
    for (i = 0; i < nphy; i++) {
        if (phylist[i] == IEEE80211_MODE_AUTO) {
            vap->iv_des_modecaps = (1 << IEEE80211_MODE_AUTO);
            return 0;
        }
        
        if (!IEEE80211_SUPPORT_PHY_MODE(vap->iv_ic, phylist[i])) {
            return -EINVAL;
        }

        vap->iv_des_modecaps |= (1 << phylist[i]);
    }

    return 0;
}

int
wlan_get_desired_phylist(wlan_if_t vaphandle,
                         enum ieee80211_phymode *phylist,
                         u_int16_t *nphy,
                         u_int16_t len)
{
    struct ieee80211vap *vap = vaphandle;
    enum ieee80211_phymode m;
    u_int16_t count;

    if (len < 1) {
        *nphy = IEEE80211_MODE_MAX;
        return -EOVERFLOW;
    }

    /* return AUTO if we accept any PHY modes */
    if (IEEE80211_ACCEPT_ANY_PHY_MODE(vap)) {
        phylist[0] = IEEE80211_MODE_AUTO;
        *nphy = 1;
        return 0;
    }

    count = 0;
    for (m = IEEE80211_MODE_AUTO + 1; m < IEEE80211_MODE_MAX; m++) {
        if (IEEE80211_ACCEPT_PHY_MODE(vap, m)) {
            /* is input big enough */
            if (len < (count+1))
                return -EINVAL;

            phylist[count++] = m;
        }
    }

    *nphy = count;
    return 0;
}

enum ieee80211_phymode
wlan_get_desired_phymode(wlan_if_t vaphandle)
{
    struct ieee80211vap *vap = vaphandle;
    return vap->iv_des_mode;
}

int
wlan_set_desired_phymode(wlan_if_t vaphandle, enum ieee80211_phymode mode)
{
    struct ieee80211vap *vap = vaphandle;
    
    if (! IEEE80211_SUPPORT_PHY_MODE(vap->iv_ic, mode))
        return -EINVAL;

    /* in multivap case, should we check if this is in agreement with other vaps */
    vap->iv_des_mode = mode;

    return 0;
}

int
wlan_set_desired_ibsschan(wlan_if_t vaphandle, int channum)
{
    struct ieee80211vap *vap = vaphandle;
    
    if (channum > IEEE80211_CHAN_MAX)
        return -EINVAL;

    vap->iv_des_ibss_chan = channum;

    return 0;
}


enum ieee80211_phymode
wlan_get_current_phymode(wlan_if_t vaphandle)
{
    return ieee80211_get_current_phymode(vaphandle->iv_ic);
}

enum ieee80211_phymode
wlan_get_bss_phymode(wlan_if_t vaphandle)
{
    return ieee80211_chan2mode(vaphandle->iv_bsschan);
}

wlan_chan_t
wlan_get_current_channel(wlan_if_t vaphandle, bool hwChan)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    /*
     * If the VAP is in RUN state, return the current channel.
     * Otherwise, return the desired channel of the desired phymode.
     */
    if (ieee80211_vap_ready_is_set(vap) || hwChan) {
        return ic->ic_curchan;
    } else {
        return vap->iv_des_chan[vap->iv_des_mode];
    }
}

wlan_chan_t 
wlan_get_bss_channel(wlan_if_t vaphandle)
{
    struct ieee80211vap *vap = vaphandle;

    return vap->iv_bsschan;
}


int
wlan_get_channel_list(wlan_dev_t devhandle, enum ieee80211_phymode phymode,
                      wlan_chan_t chanlist[], u_int32_t n)
{
    struct ieee80211com *ic = devhandle;
    struct ieee80211_channel *chan;
    int i, nchan = 0;

    for (i = 0; i < ic->ic_nchans; i++) {
        chan = &ic->ic_channels[i];

        /*
         * only active channels
         */
        if (isclr(ic->ic_chan_avail, chan->ic_ieee)) {
            continue;
        }

        if (phymode == IEEE80211_MODE_AUTO ||
            phymode == ieee80211_chan2mode(chan)) {
            /*
             * found a matching channel
             */
            if (n < (nchan + 1))
                return -EOVERFLOW;
            
            chanlist[nchan++] = chan;
        }
    }

    return nchan;
}

void
wlan_get_noise_floor(wlan_if_t vaphandle, int16_t *nfBuf)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    ic->ic_get_chainnoisefloor(ic, ic->ic_curchan, nfBuf);
}

int16_t
wlan_get_chan_noise_floor(wlan_if_t vaphandle, u_int16_t freq, u_int32_t flags)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_channel chan;

    chan.ic_freq  = freq;
    chan.ic_flags = flags;
    
    return ic->ic_get_noisefloor(ic, &chan);
}

u_int32_t
wlan_channel_frequency(wlan_chan_t chan)
{
    return (chan == IEEE80211_CHAN_ANYC) ?  IEEE80211_CHAN_ANY : chan->ic_freq;
}

u_int32_t
wlan_channel_ieee(wlan_chan_t chan)
{
    return (chan == IEEE80211_CHAN_ANYC) ?  IEEE80211_CHAN_ANY : chan->ic_ieee;
}

enum ieee80211_phymode
wlan_channel_phymode(wlan_chan_t chan)
{
    return ieee80211_chan2mode(chan);
}

int8_t
wlan_channel_maxpower(wlan_chan_t chan)
{
    return chan->ic_maxregpower;
}

u_int32_t
wlan_channel_flags(wlan_chan_t chan)
{
    return chan->ic_flags;
}

bool
wlan_channel_is_passive(wlan_chan_t chan)
{
    return (IEEE80211_IS_CHAN_PASSIVE(chan));
}

bool
wlan_channel_is_5GHzOdd(wlan_chan_t chan)
{
    return (IEEE80211_IS_CHAN_ODD(chan));
}

bool
wlan_channel_is_dfs(wlan_chan_t chan, bool flagOnly)
{
    if (flagOnly)
        return (IEEE80211_IS_CHAN_DFSFLAG(chan));
    else
        return (IEEE80211_IS_CHAN_DFS(chan));
}


/*
 * Auto Channel Select handler used for interface up.
 */
static void spectral_eacs_event_handler(void *arg, wlan_chan_t channel)  
{
    struct ieee80211vap *vap = (struct ieee80211vap *)arg;
    int chan = wlan_channel_ieee(channel);
    int error = 0;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACS,"%s channel selected is %d\n", __func__,chan);
    error = wlan_set_channel(vap, chan);
    if (error !=0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                "%s : failed to set channel with error code %d\n",
                __func__, error);
    }
    wlan_autoselect_unregister_event_handler(vap, &spectral_eacs_event_handler, vap);
}


int
wlan_set_channel(wlan_if_t vaphandle, int chan)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_channel *channel;

    if (chan > IEEE80211_CHAN_MAX)
        return -EINVAL;
    
    if (chan == IEEE80211_CHAN_ANY) {
        
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            /* allow IEEE80211_CHAN_ANYC for auto channel select in AP mode*/
            vap->iv_des_chan[vap->iv_des_mode] = IEEE80211_CHAN_ANYC;
            /* Do not trigger EACS before vap ready */
            if (ieee80211_vap_ready_is_set(vap)) {
              wlan_autoselect_register_event_handler(vap, &spectral_eacs_event_handler, (void *)vap);
              wlan_autoselect_find_infra_bss_channel(vap);
            }
            return 0;
        } else {
            /* select the desired channel for the desired PHY mode */
            channel = vap->iv_des_chan[vap->iv_des_mode];
            ASSERT(channel != IEEE80211_CHAN_ANYC);
        }
    } else {
        /*
         * find the corresponding channel object for the desired PHY mode.
         */
        channel = ieee80211_find_dot11_channel(ic, chan, vap->iv_des_mode | ic->ic_chanbwflag);
        if (channel == NULL) {
            channel = ieee80211_find_dot11_channel(ic, chan, IEEE80211_MODE_AUTO);
            if (channel == NULL)
                return -EINVAL;
        }
    }

    if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
        (ieee80211_check_chan_mode_consistency(ic,vap->iv_des_mode,channel))) {
            return -EINVAL;
    }

    /* don't allow to change to channel with radar found */
    if (IEEE80211_IS_CHAN_RADAR(channel)) {
        return -EINVAL;
    }

    /* mark desired channel */
    vap->iv_des_chan[vap->iv_des_mode] = channel;

    /*
     * Do a channel change only when we're already in the RUN or DFSWAIT state. 
     * The MLME will pickup the desired channel later.
     */

    /*
     * TBD: If curchan = channel, still need to set channel again to pass 
     * SendRecv_ext in ndistest.
     */
    if (ieee80211_vap_ready_is_set(vap) || ieee80211_vap_dfswait_is_set(vap) ||
            (ic->cw_inter_found) ) {// && ic->ic_curchan != channel) {
        ieee80211_set_channel(ic, channel);
        vap->iv_bsschan = ic->ic_curchan;
        /* This is needed to make the beacon re-initlized */
        vap->channel_change_done = 1;

#if ATH_SUPPORT_VOW_DCS
        if ( (ic->cw_inter_found) && (ieee80211_vap_ready_is_set(vap) ||
                    ieee80211_vap_dfswait_is_set(vap) )) {
            if (ieee80211_sta_power_unpause(vap->iv_pwrsave_sta) != EOK) {
                /*
                 * No need for a sophisticated error handling here. 
                 * If transmission of fake wakeup fails, subsequent traffic sent 
                 * with PM bit clear will have the same effect.
                 */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE, "%s failed to send fakewakeup\n",
                        __func__);
            }
        }
#endif


        IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE, "%s switch channel %d\n",
                __func__, ic->ic_curchan->ic_ieee);
    }

    return 0;
}

#ifdef ATH_SUPPORT_HTC
int
wlan_reset_iv_chan(wlan_if_t vaphandle, int chan)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_channel *channel;

    if ((chan != 1) && (chan != 6) && (chan != 11))
        return -EINVAL;

    /*
     * find the corresponding channel object for the desired PHY mode.
     */
    channel = ieee80211_find_dot11_channel(ic, chan, vap->iv_des_mode);
    if (channel == NULL) {
        channel = ieee80211_find_dot11_channel(ic, chan, IEEE80211_MODE_AUTO);
        if (channel == NULL) {
            return -EINVAL;
        }
    }
    vap->iv_bsschan = channel;
    return 0;
}
#endif

