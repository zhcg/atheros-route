/* userverify.c - user verify and relative operation
 * Author: bowenguo
 * Date:   2009-02-20
 */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

//#define _DEBUG
#include "debug.h"
#include "defs.h"
#include "webuserman.h"
#include "flock.h"

extern int errno;
/* successfully return 0, else -1, and strsplitted will be null string */
static int strsplit(const char * const strorgin, /* need to split string */
                    char * strsplitted,          /* splitted string */
                    unsigned int length,         /* strsplit buffer length */
                    int token,                   /* split token  */
                    unsigned int index)          /* get substring index */
{
    int i = 0;
    char * prev, * next;
    prev = (char *)strorgin;
    next = (char *)strorgin;

    memset(strsplitted, '\0', length);

    for(; i < index - 1; i++)
    {
        prev = strchr(prev, token);
        if(prev == NULL)
        {
            if(index == 1)
                prev = (char *)strorgin;
            else
                return -1;
        }
        else
            while(*(++prev) == token);
    }

    if((next = strchr(prev, token)) != NULL)
    {
        if(length < next - prev + 1) /* buffer length not enough */
            return -1;
        memcpy(strsplitted, prev, next - prev);
        DBG((stderr, "===> strsplitted = %s", strsplitted));
        return 0;
    }
    else
    {
        if(length < strlen(prev) + 1) /* buffer length not enough */
            return -1;
        strcpy(strsplitted, prev);
        DBG((stderr, "===> strsplitted = %s", strsplitted));
        return 0;
    }
}

/* find specified record, if timeout, delete it, else refresh time to now.
 * and if not find, not any change */
int shttpdext_timeoutverify(const char * const path,
                            const char * const remote_addr,
                            int timeval)
{
#define LINE_LEN 256
    DBG((stderr, "===> Enter %s", __FUNCTION__));
    
    if((path == NULL) || (remote_addr == NULL) )
        return -1;

    FILE * fp = NULL;
    FILE * fptmp = NULL;

    char line[LINE_LEN];
    char value[LINE_LEN];
    char tmp[FILENAME_MAX];
    
    if((fp = fopen(path, "r")) == NULL)
    {
        DBG((stderr, "===> Open file %s error: %s", path, strerror(errno)));
        goto out;
    }

    sprintf(tmp, "%s.tmp", path);
    if((fptmp = fopen(tmp, "w+")) == NULL)
    {
        DBG((stderr, "===> Open file %s error: %s", tmp, strerror(errno)));
        goto out;
    }

    while(fgets(line, LINE_LEN, fp) != NULL)
    {
        if(strstr(line, remote_addr) != NULL)
        {
            if((strsplit(line, value, LINE_LEN, ':', 4) != -1) && (strlen(value) != 0))
            {
                DBG((stderr, "===> time = %d, now = %d ", time(NULL), atoi(value)));
                if(time(NULL) - atoi(value) >= timeval) /* expired */
                {
                    continue;
                }
                else            /* refresh time */
                {
                    int i = 1;
                    for(; i < 4; i++)
                    {
                        if(strsplit(line, value, LINE_LEN, ':', i) == -1)
                            goto out;

                        fprintf(fptmp, "%s:", value);
                        DBG((stderr, "===> value = %s", value));
                    }
                    fprintf(fptmp, "%d\n", time(NULL));
                    continue;
                }
            }
        }
        fprintf(fptmp, "%s", line);
        DBG((stderr, "===> line = %s", line));
    }

    fclose(fp);
    fclose(fptmp);
    if(rename(tmp, path) == -1)
    {
    	DBG((stderr, "===> Rename file error: %s", strerror(errno)));
        return -1;
    }
    return 0;

  out:
    if(fp != NULL)
        fclose(fp);
    if(fptmp != NULL)
        fclose(fptmp);

        return -1;
#undef LINE_LEN
}
int locateItem(struct loginInfo *Info, char Ip[32])
{
    int ICount = 0;

    if(Info == NULL)
        return 0;

    struct loginOneItem *Item = (struct loginOneItem *)Info->data;
    while(ICount < Info->ItemCount) {
        if(!strcmp((Item + ICount)->Ip, Ip)) 
            return (ICount+1);
        ICount++;
    }
    return 0;
}

#define FIVE_MINUTES          (5 * 60)
/* find user record in specified session file, if not find, return -1, else return 0.
 * path - session file path, remote_addr - remote client ip address */
int shttpdext_userverify(const char * const path, const char * const remote_addr)
{
    int fd;
    char buffer[4096];
    
    if((fd = open(path, O_RDWR)) < 0)
        return -1;

    /* Fix bug: Two or more clients read the file of /tmp/sessioninfo.ses
     * may cause the competition condition. And this bug may caused 
     * the shttpd crashed. 
     */
    if(write_lock_wait(fd, 0, SEEK_SET, 0, 1000) < 0)   /* wait for 1s */
        goto err;

    memset(buffer, 0, sizeof buffer);
    if(read(fd, buffer, sizeof buffer) < (sizeof(struct loginInfo) + sizeof(struct loginOneItem) -1))
    {
        goto err;
    }

    struct loginInfo *LoginInfo = (struct loginInfo *)buffer;
    struct loginOneItem *FirstItem = (struct loginOneItem *)LoginInfo->data;
    int Location = locateItem(LoginInfo, remote_addr);
    struct loginOneItem *loginItem = 
        (LoginInfo->LoginItem > 0) ? (FirstItem + LoginInfo->LoginItem - 1) : FirstItem;

    if(LoginInfo->LoginItem == 0)   /* nobody login */
        goto err;
    else if(Location != LoginInfo->LoginItem) { /* someone login, but no me */
        if(time(NULL) - loginItem->Tm >= FIVE_MINUTES ||
                loginItem->Tm - time(NULL) > FIVE_MINUTES) {    /* the login guy had gone */
            char message[256];
            snprintf(message, sizeof message, "%s logout as %s", loginItem->Ip, loginItem->UserName);
            openlog("web", 0, LOG_DAEMON );
            syslog(LOG_ERR, message);
            closelog();
            while(Location < LoginInfo->ItemCount) {
                memcpy(loginItem + Location - 1, loginItem + Location, sizeof *loginItem);
                Location++;
            }
            LoginInfo->LoginItem = 0;
            if(LoginInfo->ItemCount > 0)
                LoginInfo->ItemCount--;
            else
                LoginInfo->ItemCount = 0;
            lseek(fd, 0L, SEEK_SET);
            write(fd, buffer, sizeof buffer);
            goto err;
        } else
            goto err;
    } else {    /* I have longin */
        loginItem->Tm = time(NULL);  /* update the login time */
        lseek(fd, 0L, SEEK_SET);
        write(fd, buffer, sizeof buffer);
    }
ret:
    write_unlock(fd, 0, SEEK_SET, 0);
    close(fd);
    return 0;
err:
    write_unlock(fd, 0, SEEK_SET, 0);
    close(fd);
    return -1;
}

/* redirect specified connect web page to other. redirect - redirect web
 * page name */
void shttpdext_redirect(struct conn *c, int status, const char * redirect)
{
    struct llhead       *lp;
    struct error_handler    *e;

    LL_FOREACH(&c->ctx->error_handlers, lp) {
        e = LL_ENTRY(lp, struct error_handler, link);

        if (e->code == status) {
            if (c->loc.io_class != NULL &&
                c->loc.io_class->close != NULL)
                c->loc.io_class->close(&c->loc);
            io_clear(&c->loc.io);
            _shttpd_setup_embedded_stream(c,
                                          e->callback, e->callback_data);
            return;
        }
    }
    
    io_clear(&c->loc.io);
    c->loc.io.head = _shttpd_snprintf(c->loc.io.buf, c->loc.io.size,
                                      "HTTP/1.1 200 OK\r\n"
                                      "Connection: close\r\n"
                                      "Content-Type: text/html\r\n"
                                      "Content-Length: %d\r\n"
                                      "\r\n"
                                      "<html>\r\n"
                                      "<head>\r\n"
                                      "<meta HTTP-EQUIV='Pragma' CONTENT='no-cache'>\r\n"
                                      "<meta http-equiv='Content-Type' content='text/html; charset=gb2312'>\r\n"
                                      "<script language=\"javascript\">\r\n"
                                      "function frmLoad() {\r\n"
                                      "url = \"%s\";\r\n"
                                      "top.location.href = url;\r\n"
                                      "}\r\n"
                                      "</script>\r\n"
                                      "</head>\r\n"
                                      "<body onLoad='frmLoad()'>\r\n"
                                      "</body>\r\n"
                                      "</html>\r\n", 280 + strlen(redirect), redirect);
    c->loc.content_len = 278 + strlen(redirect);
    c->status = status;
    _shttpd_stop_stream(&c->loc);
}
