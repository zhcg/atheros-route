/*
 * Copyright (c) 2004-2005 Sergey Lyubka <valenok@gmail.com>
 * All rights reserved
 *
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Sergey Lyubka wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.
 */

#include "defs.h"

/*
 * Log function
 */
void
_shttpd_elog(int flags, struct conn *c, const char *fmt, ...)
{
    char	date[64], buf[URI_MAX];
#ifdef IPV6_ENABLE    
    char    buffer[64];
#endif //IPV6_ENABLE
    int	len;
    FILE	*fp = c == NULL ? NULL : c->ctx->error_log;
    va_list	ap;

    /* Print to stderr */
    if (c == NULL || !IS_TRUE(c->ctx, OPT_INETD)) {
        va_start(ap, fmt);
        (void) vfprintf(stderr, fmt, ap);
        (void) fputc('\n', stderr);
        va_end(ap);
    }

    strftime(date, sizeof(date), "%a %b %d %H:%M:%S %Y", localtime(&_shttpd_current_time));

#ifdef IPV6_ENABLE    
    if (isIpv6)
    {
        if (c)
            inet_ntop(AF_INET6, &c->sa.u.sin6.sin6_addr, buffer, sizeof(buffer));

        len = _shttpd_snprintf(buf, sizeof(buf),
            "[%s] [error] [client %s] \"%s\" ",
            date, c ? buffer : "-",
            c && c->request ? c->request : "-");
    }
    else
    {
#endif //IPV6_ENABLE        
        len = _shttpd_snprintf(buf, sizeof(buf),
            "[%s] [error] [client %s] \"%s\" ",
            date, c ? inet_ntoa(c->sa.u.sin.sin_addr) : "-",
            c && c->request ? c->request : "-");
#ifdef IPV6_ENABLE
    }
#endif //IPV6_ENABLE
    
    va_start(ap, fmt);
    (void) vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
    va_end(ap);

    buf[sizeof(buf) - 1] = '\0';
    if (fp != NULL && (flags & (E_FATAL | E_LOG))) {
        (void) fprintf(fp, "%s\n", buf);
        (void) fflush(fp);
    }
    if (flags & E_FATAL)
        exit(EXIT_FAILURE);
}

void
_shttpd_log_access(FILE *fp, const struct conn *c)
{
    static const struct vec	dash = {"-", 1};
#ifdef IPV6_ENABLE    
    char buffer[64];
#endif //IPV6_ENABLE

    const struct vec	*user = &c->ch.user.v_vec;
    const struct vec	*referer = &c->ch.referer.v_vec;
    const struct vec	*user_agent = &c->ch.useragent.v_vec;
    char			date[64], buf[URI_MAX], *q1 = "\"", *q2 = "\"";

    if (user->len == 0)
        user = &dash;

    if (referer->len == 0) {
        referer = &dash;
        q1 = "";
    }

    if (user_agent->len == 0) {
        user_agent = &dash;
        q2 = "";
    }

    (void) strftime(date, sizeof(date), "%d/%b/%Y:%H:%M:%S",
    		localtime(&c->birth_time));

#ifdef IPV6_ENABLE
    if (isIpv6)
    {
        inet_ntop(AF_INET6, &c->sa.u.sin6.sin6_addr, buffer, sizeof(buffer));

        (void) _shttpd_snprintf(buf, sizeof(buf),
            "%s - %.*s [%s %+05d] \"%s\" %d %lu %s%.*s%s %s%.*s%s",
            buffer, user->len, user->ptr,
            date, _shttpd_tz_offset, c->request ? c->request : "-",
            c->status, (unsigned long) c->loc.io.total,
            q1, referer->len, referer->ptr, q1,
            q2, user_agent->len, user_agent->ptr, q2);
    }
    else
    {
#endif //IPV6_ENABLE        
        (void) _shttpd_snprintf(buf, sizeof(buf),
            "%s - %.*s [%s %+05d] \"%s\" %d %lu %s%.*s%s %s%.*s%s",
            inet_ntoa(c->sa.u.sin.sin_addr), user->len, user->ptr,
            date, _shttpd_tz_offset, c->request ? c->request : "-",
            c->status, (unsigned long) c->loc.io.total,
            q1, referer->len, referer->ptr, q1,
            q2, user_agent->len, user_agent->ptr, q2);
#ifdef IPV6_ENABLE
    }
#endif //IPV6_ENABLE
    
    if (fp != NULL) {
        (void) fprintf(fp, "%s\n", buf);
        (void) fflush(fp);
    }
}
