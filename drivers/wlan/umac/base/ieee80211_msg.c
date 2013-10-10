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

void ieee80211com_note(struct ieee80211com *ic, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE];
     va_list                ap;

     va_start(ap, fmt);
     vsnprintf (tmp_buf,OS_TEMP_BUF_SIZE, fmt, ap);
     va_end(ap);
     printk("%s", tmp_buf);

     /* Ignore trace messages sent before IC is fully initialized. */
     if (ic->ic_log_text != NULL) {
         ic->ic_log_text(ic,tmp_buf);
     }
}

/*
 * assume vsnprintf and snprintf are supported by the platform.
 */

void ieee80211_note(struct ieee80211vap *vap, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE],*tmp;
     va_list                ap;
     struct ieee80211com    *ic=vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "vap-%d: ", vap->iv_unit);
     va_start(ap, fmt);
     vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s",tmp_buf);
     ic->ic_log_text(ic,tmp_buf);
     OS_LOG_DBGPRINT("%s\n", tmp_buf);	
}

void ieee80211_note_mac(struct ieee80211vap *vap, u_int8_t *mac,const  char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE],*tmp;
     va_list                ap;
     struct ieee80211com    *ic=vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "vap-%d: [%s]", vap->iv_unit,ether_sprintf(mac));
     va_start(ap, fmt);
     vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s",tmp_buf);
     ic->ic_log_text(ic,tmp_buf);
     OS_LOG_DBGPRINT("%s\n", tmp_buf);	
}

void ieee80211_note_frame(struct ieee80211vap *vap,
                                    struct ieee80211_frame *wh,const  char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE],*tmp;
     va_list                ap;
     struct ieee80211com    *ic=vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "vap-%d: [%s]", vap->iv_unit,
               ether_sprintf(ieee80211vap_getbssid(vap,wh)));
     va_start(ap, fmt);
     vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s",tmp_buf);
     ic->ic_log_text(ic,tmp_buf);
     OS_LOG_DBGPRINT("%s\n", tmp_buf);	
}


void ieee80211_discard_frame(struct ieee80211vap *vap,
                                    const struct ieee80211_frame *wh, const char *type, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE],*tmp;
     va_list                ap;
     struct ieee80211com    *ic=vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "[vap-%d: %s] discard %s frame ", vap->iv_unit,
               ether_sprintf(ieee80211vap_getbssid(vap,wh)), type);
     va_start(ap, fmt);
     vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s",tmp_buf);
     ic->ic_log_text(ic,tmp_buf);
}

void ieee80211_discard_ie(struct ieee80211vap *vap, const char *type, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE],*tmp;
     va_list                ap;
     struct ieee80211com    *ic=vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "vap-%d:  discard %s information element ", vap->iv_unit, type);
     va_start(ap, fmt);
     vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s",tmp_buf);
     ic->ic_log_text(ic,tmp_buf);
}


void ieee80211_discard_mac(struct ieee80211vap *vap, u_int8_t *mac, const char *type, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE],*tmp;
     va_list                ap;
     struct ieee80211com    *ic=vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "[vap-%d: %s] discard %s frame ", vap->iv_unit,
               ether_sprintf(mac), type);
     va_start(ap, fmt);
     vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s",tmp_buf);
     ic->ic_log_text(ic,tmp_buf);
     OS_LOG_DBGPRINT("%s\n", tmp_buf);	
}


